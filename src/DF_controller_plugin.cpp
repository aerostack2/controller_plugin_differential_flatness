/*!*******************************************************************************************
 *  \file       DF_controller_plugin.cpp
 *  \brief      Differential flatness controller plugin for the Aerostack framework.
 *  \authors    Miguel Fernández Cortizas
 *              Rafael Pérez Seguí
 *              Pedro Arias Pérez
 *              David Pérez Saura
 *
 *  \copyright  Copyright (c) 2022 Universidad Politécnica de Madrid
 *              All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ********************************************************************************/

#include "DF_controller_plugin.hpp"

namespace controller_plugin_differential_flatness {

void Plugin::ownInitialize() {
  flags_.parameters_read = false;
  flags_.state_received  = false;
  flags_.ref_received    = false;

  pid_handler_ = std::make_shared<pid_controller::PIDController3D>();

  tf_handler_ = std::make_shared<as2::tf::TfHandler>(node_ptr_);

  parameters_to_read_ = std::vector<std::string>(parameters_list_);

  reset();
  return;
};

bool Plugin::updateParams(const std::vector<std::string> &_params_list) {
  auto result = parametersCallback(node_ptr_->get_parameters(_params_list));
  return result.successful;
};

void Plugin::checkParamList(const std::string &param,
                            std::vector<std::string> &_params_list,
                            bool &_all_params_read) {
  if (find(_params_list.begin(), _params_list.end(), param) != _params_list.end()) {
    // Remove the parameter from the list of parameters to be read
    _params_list.erase(std::remove(_params_list.begin(), _params_list.end(), param),
                       _params_list.end());
  };
  if (_params_list.size() == 0) {
    _all_params_read = true;
  }
};

rcl_interfaces::msg::SetParametersResult Plugin::parametersCallback(
    const std::vector<rclcpp::Parameter> &parameters) {
  rcl_interfaces::msg::SetParametersResult result;
  result.successful = true;
  result.reason     = "success";

  for (auto &param : parameters) {
    std::string param_name = param.get_name();

    if (param_name == "mass") {
      mass_ = param.get_value<double>();
      if (!flags_.parameters_read) {
        checkParamList(param_name, parameters_to_read_, flags_.parameters_read);
      }
    } else {
      std::string controller    = param_name.substr(0, param_name.find("."));
      std::string param_subname = param_name.substr(param_name.find(".") + 1);

      if (controller == "trajectory_control") {
        updateDFParameter(param_subname, param);
        if (!flags_.parameters_read) {
          checkParamList(param_name, parameters_to_read_, flags_.parameters_read);
        }
      }
    }
  }
  return result;
}

void Plugin::updateDFParameter(const std::string &_parameter_name,
                               const rclcpp::Parameter &_param) {
  if (_parameter_name == "reset_integral") {
    pid_handler_->setResetIntegralSaturationFlag(_param.get_value<bool>());
  } else if (_parameter_name == "antiwindup_cte") {
    pid_handler_->setAntiWindup(_param.get_value<double>());
  } else if (_parameter_name == "alpha") {
    pid_handler_->setAlpha(_param.get_value<double>());
  } else if (_parameter_name == "kp.x") {
    pid_handler_->setGainKpX(_param.get_value<double>());
  } else if (_parameter_name == "kp.y") {
    pid_handler_->setGainKpY(_param.get_value<double>());
  } else if (_parameter_name == "kp.z") {
    pid_handler_->setGainKpZ(_param.get_value<double>());
  } else if (_parameter_name == "ki.x") {
    pid_handler_->setGainKiX(_param.get_value<double>());
  } else if (_parameter_name == "ki.y") {
    pid_handler_->setGainKiY(_param.get_value<double>());
  } else if (_parameter_name == "ki.z") {
    pid_handler_->setGainKiZ(_param.get_value<double>());
  } else if (_parameter_name == "kd.x") {
    pid_handler_->setGainKdX(_param.get_value<double>());
  } else if (_parameter_name == "kd.y") {
    pid_handler_->setGainKdY(_param.get_value<double>());
  } else if (_parameter_name == "kd.z") {
    pid_handler_->setGainKdZ(_param.get_value<double>());
  } else if (_parameter_name == "roll_control.kp") {
    Kp_ang_mat_(0, 0) = _param.get_value<double>();
  } else if (_parameter_name == "pitch_control.kp") {
    Kp_ang_mat_(1, 1) = _param.get_value<double>();
  } else if (_parameter_name == "yaw_control.kp") {
    Kp_ang_mat_(2, 2) = _param.get_value<double>();
  }
  return;
}

void Plugin::reset() {
  resetState();
  resetReferences();
  resetCommands();
  pid_handler_->resetController();
}

void Plugin::resetState() {
  uav_state_ = UAV_state();
  return;
}

void Plugin::resetReferences() {
  control_ref_.position     = uav_state_.position;
  control_ref_.velocity     = Eigen::Vector3d::Zero();
  control_ref_.acceleration = Eigen::Vector3d::Zero();

  control_ref_.yaw =
      Eigen::Vector3d(as2::frame::getYawFromQuaternion(uav_state_.attitude_state), 0, 0);
  return;
}

void Plugin::resetCommands() {
  control_command_.PQR    = Eigen::Vector3d::Zero();
  control_command_.thrust = 0.0;
  return;
}

void Plugin::updateState(const geometry_msgs::msg::PoseStamped &pose_msg,
                         const geometry_msgs::msg::TwistStamped &twist_msg) {
  uav_state_.position_header = pose_msg.header;
  uav_state_.position =
      Eigen::Vector3d(pose_msg.pose.position.x, pose_msg.pose.position.y, pose_msg.pose.position.z);

  geometry_msgs::msg::TwistStamped twist_msg_flu = twist_msg;
  geometry_msgs::msg::TwistStamped twist_msg_enu;
  twist_msg_enu       = tf_handler_->convert(twist_msg_flu, enu_frame_id_);
  uav_state_.velocity = Eigen::Vector3d(twist_msg_enu.twist.linear.x, twist_msg_enu.twist.linear.y,
                                        twist_msg_enu.twist.linear.z);

  uav_state_.attitude_state =
      tf2::Quaternion(pose_msg.pose.orientation.x, pose_msg.pose.orientation.y,
                      pose_msg.pose.orientation.z, pose_msg.pose.orientation.w);

  flags_.state_received = true;
  return;
};

void Plugin::updateReference(const trajectory_msgs::msg::JointTrajectoryPoint &traj_msg) {
  if (control_mode_in_.control_mode != as2_msgs::msg::ControlMode::TRAJECTORY) {
    return;
  }

  control_ref_.position =
      Eigen::Vector3d(traj_msg.positions[0], traj_msg.positions[1], traj_msg.positions[2]);

  control_ref_.velocity =
      Eigen::Vector3d(traj_msg.velocities[0], traj_msg.velocities[1], traj_msg.velocities[2]);

  control_ref_.acceleration = Eigen::Vector3d(traj_msg.accelerations[0], traj_msg.accelerations[1],
                                              traj_msg.accelerations[2]);
  control_ref_.yaw =
      Eigen::Vector3d(traj_msg.positions[3], traj_msg.velocities[3], traj_msg.accelerations[3]);

  flags_.ref_received = true;
  return;
};

bool Plugin::setMode(const as2_msgs::msg::ControlMode &in_mode,
                     const as2_msgs::msg::ControlMode &out_mode) {
  if (in_mode.control_mode == as2_msgs::msg::ControlMode::HOVER) {
    control_mode_in_.control_mode    = in_mode.control_mode;
    control_mode_in_.yaw_mode        = as2_msgs::msg::ControlMode::YAW_ANGLE;
    control_mode_in_.reference_frame = as2_msgs::msg::ControlMode::LOCAL_ENU_FRAME;
  } else {
    flags_.ref_received   = false;
    flags_.state_received = false;
    control_mode_in_      = in_mode;
  }

  control_mode_out_ = out_mode;
  reset();

  return true;
};

bool Plugin::computeOutput(const double &dt,
                           geometry_msgs::msg::PoseStamped &pose,
                           geometry_msgs::msg::TwistStamped &twist,
                           as2_msgs::msg::Thrust &thrust) {
  if (!flags_.state_received) {
    auto &clk = *node_ptr_->get_clock();
    RCLCPP_WARN_THROTTLE(node_ptr_->get_logger(), clk, 5000, "State not received yet");
    return false;
  }

  if (!flags_.parameters_read) {
    auto &clk = *node_ptr_->get_clock();
    RCLCPP_WARN_THROTTLE(node_ptr_->get_logger(), clk, 5000, "Parameters not read yet");
    for (auto &param : parameters_to_read_) {
      RCLCPP_WARN(node_ptr_->get_logger(), "Parameter %s not read yet", param.c_str());
    }
    return false;
  }

  if (!flags_.ref_received) {
    auto &clk = *node_ptr_->get_clock();
    RCLCPP_WARN_THROTTLE(node_ptr_->get_logger(), clk, 5000,
                         "State changed, but ref not recived yet");
    return false;
  }

  RCLCPP_INFO(node_ptr_->get_logger(), "dt: %f", dt);

  resetCommands();

  switch (control_mode_in_.yaw_mode) {
    case as2_msgs::msg::ControlMode::YAW_ANGLE: {
      break;
    }
    case as2_msgs::msg::ControlMode::YAW_SPEED: {
      tf2::Matrix3x3 m(uav_state_.attitude_state);
      double roll, pitch, yaw;
      m.getRPY(roll, pitch, yaw);
      control_ref_.yaw.x() = yaw + control_ref_.yaw.y() * dt;
      break;
    }
    default:
      auto &clk = *node_ptr_->get_clock();
      RCLCPP_ERROR_THROTTLE(node_ptr_->get_logger(), clk, 5000, "Unknown yaw mode");
      return false;
      break;
  }

  switch (control_mode_in_.control_mode) {
    case as2_msgs::msg::ControlMode::TRAJECTORY: {
      // TODO: Change twist to odom frame
      control_command_ = computeTrajectoryControl(dt, uav_state_.position, uav_state_.velocity,
                                                  uav_state_.attitude_state, control_ref_.position,
                                                  control_ref_.velocity, control_ref_.acceleration,
                                                  control_ref_.yaw.x());
      break;
    }
    default:
      auto &clk = *node_ptr_->get_clock();
      RCLCPP_ERROR_THROTTLE(node_ptr_->get_logger(), clk, 5000, "Unknown control mode");
      return false;
      break;
  }

  return getOutput(twist, thrust);
}

Eigen::Vector3d Plugin::getForce(const double &_dt,
                                 const Eigen::Vector3d &_pos_state,
                                 const Eigen::Vector3d &_vel_state,
                                 const Eigen::Vector3d &_pos_reference,
                                 const Eigen::Vector3d &_vel_reference,
                                 const Eigen::Vector3d &_acc_reference) {
  // Compute the error force contribution
  // Eigen::Vector3d force_error =
  //     pid_handler_->computeControl(_dt, _pos_state, _pos_reference, _vel_state, _vel_reference);
  Eigen::Vector3d force_error =
      pid_handler_->computeControl(_pos_state, _pos_reference, _vel_state, _vel_reference);

  // Compute acceleration reference contribution
  Eigen::Vector3d force_acceleration = mass_ * _acc_reference;

  // Compute gravity compensation
  Eigen::Vector3d force_gravity = mass_ * gravitational_accel_;

  // Return desired force with the gravity compensation
  Eigen::Vector3d desired_force = force_error + force_acceleration + force_gravity;
  return desired_force;
}

Acro_command Plugin::computeTrajectoryControl(const double &_dt,
                                              const Eigen::Vector3d &_pos_state,
                                              const Eigen::Vector3d &_vel_state,
                                              const tf2::Quaternion &_attitude_state,
                                              const Eigen::Vector3d &_pos_reference,
                                              const Eigen::Vector3d &_vel_reference,
                                              const Eigen::Vector3d &_acc_reference,
                                              const double &_yaw_angle_reference) {
  Eigen::Vector3d desired_force =
      getForce(_dt, _pos_state, _vel_state, _pos_reference, _vel_reference, _acc_reference);

  // Compute the desired attitude
  tf2::Matrix3x3 rot_matrix_tf2(_attitude_state);

  Eigen::Matrix3d rot_matrix;
  rot_matrix << rot_matrix_tf2[0][0], rot_matrix_tf2[0][1], rot_matrix_tf2[0][2],
      rot_matrix_tf2[1][0], rot_matrix_tf2[1][1], rot_matrix_tf2[1][2], rot_matrix_tf2[2][0],
      rot_matrix_tf2[2][1], rot_matrix_tf2[2][2];

  Eigen::Vector3d xc_des(cos(_yaw_angle_reference), sin(_yaw_angle_reference), 0);

  Eigen::Vector3d zb_des = desired_force.normalized();
  Eigen::Vector3d yb_des = zb_des.cross(xc_des).normalized();
  Eigen::Vector3d xb_des = yb_des.cross(zb_des).normalized();

  // Compute the rotation matrix desidered
  Eigen::Matrix3d R_des;
  R_des.col(0) = xb_des;
  R_des.col(1) = yb_des;
  R_des.col(2) = zb_des;

  // Compute the rotation matrix error
  Eigen::Matrix3d Mat_e_rot = (R_des.transpose() * rot_matrix - rot_matrix.transpose() * R_des);

  Eigen::Vector3d V_e_rot(Mat_e_rot(2, 1), Mat_e_rot(0, 2), Mat_e_rot(1, 0));
  Eigen::Vector3d E_rot = (1.0f / 2.0f) * V_e_rot;

  Acro_command acro_command;
  acro_command.thrust = (float)desired_force.dot(rot_matrix.col(2).normalized());
  acro_command.PQR    = -Kp_ang_mat_ * E_rot;

  return acro_command;
}

bool Plugin::getOutput(geometry_msgs::msg::TwistStamped &twist_msg,
                       as2_msgs::msg::Thrust &thrust_msg) {
  twist_msg.header.stamp    = node_ptr_->now();
  twist_msg.header.frame_id = flu_frame_id_;
  twist_msg.twist.angular.x = control_command_.PQR.x();
  twist_msg.twist.angular.y = control_command_.PQR.y();
  twist_msg.twist.angular.z = control_command_.PQR.z();

  thrust_msg.header.stamp    = node_ptr_->now();
  thrust_msg.header.frame_id = flu_frame_id_;
  thrust_msg.thrust          = control_command_.thrust;
  return true;
};

}  // namespace controller_plugin_differential_flatness

#include <pluginlib/class_list_macros.hpp>
PLUGINLIB_EXPORT_CLASS(controller_plugin_differential_flatness::Plugin,
                       controller_plugin_base::ControllerBase)

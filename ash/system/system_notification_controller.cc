// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/system_notification_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/system/caps_lock_notification_controller.h"
#include "ash/system/cast/cast_notification_controller.h"
#include "ash/system/gesture_education/gesture_education_notification_controller.h"
#include "ash/system/microphone_mute/microphone_mute_notification_controller.h"
#include "ash/system/network/auto_connect_notifier.h"
#include "ash/system/network/cellular_setup_notifier.h"
#include "ash/system/network/managed_sim_lock_notifier.h"
#include "ash/system/network/wifi_toggle_notification_controller.h"
#include "ash/system/power/power_notification_controller.h"
#include "ash/system/privacy/screen_security_notification_controller.h"
#include "ash/system/session/session_limit_notification_controller.h"
#include "ash/system/tracing_notification_controller.h"
#include "ash/system/update/update_notification_controller.h"
#include "ui/message_center/message_center.h"

namespace ash {

SystemNotificationController::SystemNotificationController()
    : auto_connect_(std::make_unique<AutoConnectNotifier>()),
      caps_lock_(std::make_unique<CapsLockNotificationController>()),
      cast_(std::make_unique<CastNotificationController>()),
      cellular_setup_notifier_(std::make_unique<ash::CellularSetupNotifier>()),
      gesture_education_(
          std::make_unique<GestureEducationNotificationController>()),
      power_(std::make_unique<PowerNotificationController>(
          message_center::MessageCenter::Get())),
      screen_security_(
          std::make_unique<ScreenSecurityNotificationController>()),
      session_limit_(std::make_unique<SessionLimitNotificationController>()),
      tracing_(std::make_unique<TracingNotificationController>()),
      update_(std::make_unique<UpdateNotificationController>()),
      wifi_toggle_(std::make_unique<WifiToggleNotificationController>()) {
  if (features::IsMicMuteNotificationsEnabled()) {
    microphone_mute_ = std::make_unique<MicrophoneMuteNotificationController>();
  }

  if (features::IsSimLockPolicyEnabled()) {
    managed_sim_lock_notifier_ =
        std::make_unique<ash::ManagedSimLockNotifier>();
  }
}

SystemNotificationController::~SystemNotificationController() = default;

}  // namespace ash

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/system_notification_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/system/caps_lock_notification_controller.h"
#include "ash/system/cast/cast_notification_controller.h"
#include "ash/system/do_not_disturb_notification_controller.h"
#include "ash/system/hotspot/hotspot_notifier.h"
#include "ash/system/lock_screen_notification_controller.h"
#include "ash/system/network/auto_connect_notifier.h"
#include "ash/system/network/cellular_setup_notifier.h"
#include "ash/system/network/managed_sim_lock_notifier.h"
#include "ash/system/network/wifi_toggle_notification_controller.h"
#include "ash/system/power/power_notification_controller.h"
#include "ash/system/power/power_sounds_controller.h"
#include "ash/system/privacy/privacy_indicators_controller.h"
#include "ash/system/privacy/screen_security_controller.h"
#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"
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
      do_not_disturb_(std::make_unique<DoNotDisturbNotificationController>()),
      hotspot_notifier_(std::make_unique<ash::HotspotNotifier>()),
      lock_screen_(std::make_unique<LockScreenNotificationController>()),
      managed_sim_lock_notifier_(
          std::make_unique<ash::ManagedSimLockNotifier>()),
      power_(std::make_unique<PowerNotificationController>(
          message_center::MessageCenter::Get())),
      power_sounds_(std::make_unique<PowerSoundsController>()),
      privacy_hub_(std::make_unique<PrivacyHubNotificationController>()),
      screen_security_controller_(std::make_unique<ScreenSecurityController>()),
      session_limit_(std::make_unique<SessionLimitNotificationController>()),
      tracing_(std::make_unique<TracingNotificationController>()),
      update_(std::make_unique<UpdateNotificationController>()),
      wifi_toggle_(std::make_unique<WifiToggleNotificationController>()) {
  // Privacy indicator is only enabled when Video Conference is disabled.
  if (!features::IsVideoConferenceEnabled()) {
    privacy_indicators_controller_ =
        std::make_unique<PrivacyIndicatorsController>();
  }
}

SystemNotificationController::~SystemNotificationController() = default;

}  // namespace ash

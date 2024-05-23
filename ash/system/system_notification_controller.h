// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SYSTEM_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_SYSTEM_NOTIFICATION_CONTROLLER_H_

#include <memory>

namespace ash {

class AutoConnectNotifier;
class AutoConnectNotifierTest;
class CapsLockNotificationController;
class CastNotificationController;
class CellularSetupNotifier;
class DoNotDisturbNotificationController;
class LockScreenNotificationController;
class ManagedSimLockNotifier;
class HotspotNotifier;
class PowerNotificationController;
class PowerSoundsController;
class PrivacyHubNotificationController;
class PrivacyIndicatorsController;
class ScreenSecurityController;
class SessionLimitNotificationController;
class SessionStateNotificationBlockerTest;
class TracingNotificationController;
class UpdateNotificationController;
class WifiToggleNotificationController;

// Class that owns individual notification controllers.
class SystemNotificationController {
 public:
  SystemNotificationController();

  SystemNotificationController(const SystemNotificationController&) = delete;
  SystemNotificationController& operator=(const SystemNotificationController&) =
      delete;

  ~SystemNotificationController();

  DoNotDisturbNotificationController* do_not_disturb() {
    return do_not_disturb_.get();
  }

  PrivacyHubNotificationController* privacy_hub() const {
    return privacy_hub_.get();
  }

  ScreenSecurityController* screen_security_controller() const {
    return screen_security_controller_.get();
  }

  PowerNotificationController* power_notification_controller() {
    return power_.get();
  }

 private:
  friend class AutoConnectNotifierTest;
  friend class CellularSetupNotifierTest;
  friend class ManagedSimLockNotifier;
  friend class PowerSoundsControllerTest;
  friend class PrivacyHubNotificationControllerTest;
  friend class SessionStateNotificationBlockerTest;
  friend class UpdateNotificationControllerTest;
  const std::unique_ptr<AutoConnectNotifier> auto_connect_;
  const std::unique_ptr<CapsLockNotificationController> caps_lock_;
  const std::unique_ptr<CastNotificationController> cast_;
  const std::unique_ptr<CellularSetupNotifier> cellular_setup_notifier_;
  const std::unique_ptr<DoNotDisturbNotificationController> do_not_disturb_;
  const std::unique_ptr<HotspotNotifier> hotspot_notifier_;
  const std::unique_ptr<LockScreenNotificationController> lock_screen_;
  const std::unique_ptr<ManagedSimLockNotifier> managed_sim_lock_notifier_;
  const std::unique_ptr<PowerNotificationController> power_;
  const std::unique_ptr<PowerSoundsController> power_sounds_;
  const std::unique_ptr<PrivacyHubNotificationController> privacy_hub_;
  std::unique_ptr<PrivacyIndicatorsController> privacy_indicators_controller_;
  const std::unique_ptr<ScreenSecurityController> screen_security_controller_;
  const std::unique_ptr<SessionLimitNotificationController> session_limit_;
  const std::unique_ptr<TracingNotificationController> tracing_;
  const std::unique_ptr<UpdateNotificationController> update_;
  const std::unique_ptr<WifiToggleNotificationController> wifi_toggle_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_SYSTEM_NOTIFICATION_CONTROLLER_H_

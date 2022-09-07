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
class GestureEducationNotificationController;
class CastNotificationController;
class CellularSetupNotifier;
class ManagedSimLockNotifier;
class MicrophoneMuteNotificationController;
class PowerNotificationController;
class ScreenSecurityNotificationController;
class SessionLimitNotificationController;
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

 private:
  friend class AutoConnectNotifierTest;
  friend class CellularSetupNotifierTest;
  friend class ManagedSimLockNotifier;
  friend class UpdateNotificationControllerTest;
  const std::unique_ptr<AutoConnectNotifier> auto_connect_;
  const std::unique_ptr<CapsLockNotificationController> caps_lock_;
  const std::unique_ptr<CastNotificationController> cast_;
  const std::unique_ptr<CellularSetupNotifier> cellular_setup_notifier_;
  const std::unique_ptr<GestureEducationNotificationController>
      gesture_education_;
  // TODO(b/228093904): Make |managed_sim_lock_notifier_| const during cleanup.
  std::unique_ptr<ManagedSimLockNotifier> managed_sim_lock_notifier_;
  std::unique_ptr<MicrophoneMuteNotificationController> microphone_mute_;
  const std::unique_ptr<PowerNotificationController> power_;
  const std::unique_ptr<ScreenSecurityNotificationController> screen_security_;
  const std::unique_ptr<SessionLimitNotificationController> session_limit_;
  const std::unique_ptr<TracingNotificationController> tracing_;
  const std::unique_ptr<UpdateNotificationController> update_;
  const std::unique_ptr<WifiToggleNotificationController> wifi_toggle_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_SYSTEM_NOTIFICATION_CONTROLLER_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HUMAN_PRESENCE_LOCK_ON_LEAVE_CONTROLLER_H_
#define ASH_SYSTEM_HUMAN_PRESENCE_LOCK_ON_LEAVE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/human_presence/human_presence_orientation_controller.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/dbus/hps/hps_service.pb.h"
#include "chromeos/ash/components/dbus/human_presence/human_presence_dbus_client.h"

namespace ash {

// Helper class for HumanPresenceDBusClient, responsible for
// enabling/disabling the DBus service via the client and is responsible for
// maintaining state between restarts.
class ASH_EXPORT LockOnLeaveController
    : public HumanPresenceOrientationController::Observer,
      HumanPresenceDBusClient::Observer {
 public:
  // The state of lock on leave inside DBus service that is configured. It is
  // set as kUnknown in this class on initialization. And is set to either
  // kEnable or kDisable when EnableLockOnLeave() or DisableLockOnLeave() is
  // called.
  enum class ConfiguredLockOnLeaveState {
    kUnknown,
    kEnabled,
    kDisabled,
  };

  LockOnLeaveController();
  LockOnLeaveController(const LockOnLeaveController&) = delete;
  LockOnLeaveController& operator=(const LockOnLeaveController&) = delete;

  ~LockOnLeaveController() override;

  // Enables the LockOnLeave feature inside HumanPresenceDBusClient; and it only
  // sends the method call if LockOnLeave is not enabled yet.
  void EnableLockOnLeave();
  // Disables the LockOnLeave feature inside HumanPresenceDBusClient if it is
  // currently enabled.
  void DisableLockOnLeave();

  // HumanPresenceOrientationObserver:
  void OnOrientationChanged(bool suitable_for_human_presence) override;

  // HumanPresenceDBusClient::Observer:
  void OnHpsSenseChanged(const hps::HpsResultProto&) override;
  void OnHpsNotifyChanged(const hps::HpsResultProto&) override;
  // Re-enables LockOnLeave on human presence service restart if it was enabled
  // before.
  void OnRestart() override;
  void OnShutdown() override;

 private:
  // Called when the human presence service is available.
  void OnServiceAvailable(bool service_available);

  // May disable/enable lock-on-leave based on current state.
  void ReconfigViaDbus();

  // Indicates whether the human presence service is available; it is set inside
  // OnServiceAvailable and set to false OnShutdown.
  bool service_available_ = false;

  // Records requested lock-on-leave enable state from client.
  bool want_lock_on_leave_ = false;

  // Whether the device is in physical orientation where our models are
  // accurate.
  bool suitable_for_human_presence_ = false;

  // Current configured state of LockOnLeave.
  ConfiguredLockOnLeaveState configured_state_ =
      ConfiguredLockOnLeaveState::kUnknown;

  base::ScopedObservation<HumanPresenceDBusClient,
                          HumanPresenceDBusClient::Observer>
      human_presence_observation_{this};
  base::ScopedObservation<HumanPresenceOrientationController,
                          HumanPresenceOrientationController::Observer>
      orientation_observation_{this};
  base::WeakPtrFactory<LockOnLeaveController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HUMAN_PRESENCE_LOCK_ON_LEAVE_CONTROLLER_H_

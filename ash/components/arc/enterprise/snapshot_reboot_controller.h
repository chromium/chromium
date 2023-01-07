// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_ENTERPRISE_SNAPSHOT_REBOOT_CONTROLLER_H_
#define ASH_COMPONENTS_ARC_ENTERPRISE_SNAPSHOT_REBOOT_CONTROLLER_H_

#include "ash/components/arc/enterprise/arc_snapshot_reboot_notification.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace arc {
namespace data_snapshotd {

// Maximum number of consequent reboot attempts.
extern const int kMaxRebootAttempts;

// A time delta between reboot attempts.
extern const base::TimeDelta kRebootAttemptDelay;

// This class observes the MGS state changes and requests a reboot as soon as
// possible.
class SnapshotRebootController
    : public session_manager::SessionManagerObserver {
 public:
  explicit SnapshotRebootController(
      std::unique_ptr<ArcSnapshotRebootNotification> notification);
  SnapshotRebootController(const SnapshotRebootController&) = delete;
  ~SnapshotRebootController() override;

  SnapshotRebootController& operator=(const SnapshotRebootController&) = delete;

  // session_manager::SessionManagerObserver overrides:
  void OnSessionStateChanged() override;

  base::OneShotTimer* get_timer_for_testing() { return &reboot_timer_; }

 private:
  void StartRebootTimer();
  void SetRebootTimer();
  void StopRebootTimer();
  void OnRebootTimer();
  // This callback is called once user consents to restart a device.
  // The device is requested to be restarted immediately.
  void HandleUserConsent();

  base::OneShotTimer reboot_timer_;
  int reboot_attempts_ = 0;

  std::unique_ptr<ArcSnapshotRebootNotification> notification_;

  base::WeakPtrFactory<SnapshotRebootController> weak_ptr_factory_{this};
};

}  // namespace data_snapshotd
}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_ENTERPRISE_SNAPSHOT_REBOOT_CONTROLLER_H_

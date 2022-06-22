// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SESSION_ARC_DISK_SPACE_MONITOR_H_
#define CHROME_BROWSER_ASH_ARC_SESSION_ARC_DISK_SPACE_MONITOR_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace arc {

// These thresholds are chosen based on UMA stats. (go/arcvm-virtio-blk-sparse)
// Show a pre-warning notification if free disk space is lower than this.
constexpr int64_t kDiskSpaceThresholdForPreWarning = 1LL << 30;  // 1GB

// Stop ARC and show a final warning notification if free disk space is
// lower than this.
constexpr int64_t kDiskSpaceThresholdForStoppingArc = 256LL << 20;  // 256MB

// TODO(b/233030867): Choose these values based on some logic
//                    instead of deciding them on a hunch.
// Disk space check interval used when free disk space is lower than
// kDiskSpaceThresholdForPreWarning.
constexpr base::TimeDelta kDiskSpaceCheckIntervalShort = base::Seconds(1);

// Disk space check interval used when free disk space is higher than
// kDiskSpaceThresholdForPreWarning.
constexpr base::TimeDelta kDiskSpaceCheckIntervalLong = base::Seconds(10);

// Monitors disk usage. Requests stopping ARC and/or shows a warning
// notification when device's free disk space becomes lower than a threshold.
// Used when arcvm_virtio_blk_data is enabled. (go/arcvm-virtio-blk-sparse)
// TODO(b/233030867): Delete this after we switch ARCVM to using Storage
//                    Balloons for disk space management.
class ArcDiskSpaceMonitor : public ArcSessionManagerObserver {
 public:
  ArcDiskSpaceMonitor();
  ~ArcDiskSpaceMonitor() override;

  ArcDiskSpaceMonitor(const ArcDiskSpaceMonitor&) = delete;
  ArcDiskSpaceMonitor& operator=(const ArcDiskSpaceMonitor&) = delete;

  bool IsTimerRunningForTesting() { return timer_.IsRunning(); }
  base::TimeDelta GetTimerCurrentDelayForTesting() {
    return timer_.GetCurrentDelay();
  }

  // ArcSessionManagerObserver overrides.
  void OnArcStarted() override;
  void OnArcSessionStopped(ArcStopReason stop_reason) override;

 private:
  // Schedules calling CheckDiskSpace().
  void ScheduleCheckDiskSpace(base::TimeDelta delay);

  // Checks disk usage, requests stopping ARC and/or shows a warning
  // notification based on the free disk space.
  void CheckDiskSpace();

  // Used as a callback function.
  void OnGetFreeDiskSpace(absl::optional<int64_t> reply);

  // Used for periodically calling CheckDiskSpace().
  base::OneShotTimer timer_;

  // WeakPtrFactory to use callbacks.
  base::WeakPtrFactory<ArcDiskSpaceMonitor> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SESSION_ARC_DISK_SPACE_MONITOR_H_

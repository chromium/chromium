// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SESSION_ARC_DISK_SPACE_MONITOR_H_
#define CHROME_BROWSER_ASH_ARC_SESSION_ARC_DISK_SPACE_MONITOR_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"

namespace arc {

// These thresholds are chosen based on UMA stats. (go/arcvm-virtio-blk-sparse)
// Show a pre-stop warning notification if free disk space is lower than this.
constexpr int64_t kDiskSpaceThresholdForPreStopNotification = 1LL << 30;  // 1GB

// Stop ARC and show a post-stop warning notification if free disk space is
// lower than this.
constexpr int64_t kDiskSpaceThresholdForStoppingArc = 256LL << 20;  // 256MB

// TODO(b/233030867): Choose these values based on some logic
//                    instead of deciding them on a hunch.
// Disk space check interval used when free disk space is lower than
// kDiskSpaceThresholdForPreStopNotification.
constexpr base::TimeDelta kDiskSpaceCheckIntervalShort = base::Seconds(1);

// Disk space check interval used when free disk space is higher than
// kDiskSpaceThresholdForPreStopNotification.
constexpr base::TimeDelta kDiskSpaceCheckIntervalLong = base::Seconds(10);

// A pre-stop warning notification should not be shown more than once within
// this interval.
// TODO(b/237040345): Finalize the value.
constexpr base::TimeDelta kPreStopNotificationReshowInterval = base::Minutes(2);

// Notifier ID of ArcDiskSpaceMonitor.
const char kDiskSpaceMonitorNotifierId[] = "arc_disk_space_monitor";

// Notification ID of the pre-stop warning notification.
const char kLowDiskSpacePreStopNotificationId[] = "arc_low_disk_space_pre_stop";

// Notification ID of the post-stop warning notification.
const char kLowDiskSpacePostStopNotificationId[] =
    "arc_low_disk_space_post_stop";

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
  void OnGetFreeDiskSpace(std::optional<int64_t> reply);

  // Shows a pre-stop warning notification if |is_pre_stop| is true and the
  // same notification was not shown within kPreStopNotificationReshowInterval.
  // Always shows a post-stop warning notification if |is_pre_stop| is false.
  void MaybeShowNotification(bool is_pre_stop);

  // The last time when a pre-stop warning notification was shown.
  base::Time pre_stop_notification_last_shown_time_;

  // Used for periodically calling CheckDiskSpace().
  base::OneShotTimer timer_;

  // WeakPtrFactory to use callbacks.
  base::WeakPtrFactory<ArcDiskSpaceMonitor> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SESSION_ARC_DISK_SPACE_MONITOR_H_

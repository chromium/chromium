// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_disk_space_monitor.h"

#include "base/logging.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"

namespace arc {

ArcDiskSpaceMonitor::ArcDiskSpaceMonitor() {
  ArcSessionManager::Get()->AddObserver(this);
}

ArcDiskSpaceMonitor::~ArcDiskSpaceMonitor() {
  ArcSessionManager::Get()->RemoveObserver(this);
}

void ArcDiskSpaceMonitor::OnArcStarted() {
  VLOG(1) << "ARC started. Activating ArcDiskSpaceMonitor.";

  // Calling ScheduleCheckDiskSpace(Seconds(0)) instead of CheckDiskSpace()
  // because ArcSessionManager::RequestStopOnLowDiskSpace() doesn't work if it
  // is called directly inside OnArcStarted().
  ScheduleCheckDiskSpace(base::Seconds(0));
}

void ArcDiskSpaceMonitor::OnArcSessionStopped(ArcStopReason stop_reason) {
  VLOG(1) << "ARC stopped. Deactivating ArcDiskSpaceMonitor.";
  timer_.Stop();
}

void ArcDiskSpaceMonitor::ScheduleCheckDiskSpace(base::TimeDelta delay) {
  timer_.Start(FROM_HERE, delay,
               base::BindOnce(&ArcDiskSpaceMonitor::CheckDiskSpace,
                              weak_ptr_factory_.GetWeakPtr()));
}

void ArcDiskSpaceMonitor::CheckDiskSpace() {
  ash::SpacedClient::Get()->GetFreeDiskSpace(
      "/home", base::BindOnce(&ArcDiskSpaceMonitor::OnGetFreeDiskSpace,
                              weak_ptr_factory_.GetWeakPtr()));
}

void ArcDiskSpaceMonitor::OnGetFreeDiskSpace(absl::optional<int64_t> reply) {
  if (!reply.has_value() || reply.value() < 0) {
    LOG(ERROR) << "spaced::GetFreeDiskSpace failed. "
               << "Deactivating ArcDiskSpaceMonitor.";
    return;
  }
  const int64_t free_disk_space = reply.value();

  arc::ArcSessionManager* const arc_session_manager =
      arc::ArcSessionManager::Get();
  const ArcSessionManager::State state = arc_session_manager->state();

  VLOG(1) << "ArcSessionManager::State:" << state
          << ", free_disk_space:" << free_disk_space;

  if (state != ArcSessionManager::State::ACTIVE) {
    LOG(WARNING) << "ARC is not active.";
    // No need to call ScheduleCheckDiskSpace() because
    // OnArcStarted() will trigger CheckDiskSpace() when ARC starts.
    return;
  }

  if (free_disk_space < kDiskSpaceThresholdForStoppingArc) {
    LOG(WARNING) << "Stopping ARC due to low disk space. free_disk_space:"
                 << free_disk_space;
    arc_session_manager->RequestStopOnLowDiskSpace();
    // TODO(b/233030867): Show a final warning notification.
    return;
  } else if (free_disk_space < kDiskSpaceThresholdForPreWarning) {
    // TODO(b/233030867): Show a pre-warning notification.
  }

  if (free_disk_space < kDiskSpaceThresholdForPreWarning)
    ScheduleCheckDiskSpace(kDiskSpaceCheckIntervalShort);
  else
    ScheduleCheckDiskSpace(kDiskSpaceCheckIntervalLong);
}

}  // namespace arc

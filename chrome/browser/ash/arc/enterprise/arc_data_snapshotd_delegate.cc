// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/enterprise/arc_data_snapshotd_delegate.h"

#include "chrome/browser/ash/login/chrome_restart_request.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/enterprise/arc_force_installed_apps_tracker.h"
#include "chrome/browser/ash/arc/enterprise/arc_snapshot_reboot_notification_impl.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"

namespace arc {
namespace data_snapshotd {

namespace {

// Returns true if ARC is active and running.
bool IsArcActive(arc::ArcSessionManager::State state) {
  return state == ArcSessionManager::State::ACTIVE;
}

// Returns true if ARC is stopped.
bool IsArcStopped(arc::ArcSessionManager::State state) {
  return state == ArcSessionManager::State::STOPPED;
}

}  // namespace

ArcDataSnapshotdDelegate::ArcDataSnapshotdDelegate()
    : arc_session_manager_(arc::ArcSessionManager::Get()) {
  DCHECK(arc_session_manager_);
}

ArcDataSnapshotdDelegate::~ArcDataSnapshotdDelegate() {
  if (!arc_stopped_callback_.is_null())
    NotifyArcStopped(false /* success */);
}

void ArcDataSnapshotdDelegate::RequestStopArcInstance(
    base::OnceCallback<void(bool)> stopped_callback) {
  DCHECK(!stopped_callback.is_null());
  if (!arc_stopped_callback_.is_null()) {
    // Previous request to stop ARC has not been satisfied yet.
    LOG(WARNING) << "Requested to stop ARC twice.";
    // Report failure to both callbacks.
    NotifyArcStopped(false /* success */);
    std::move(stopped_callback).Run(false /* success */);
    return;
  }
  if (!IsArcActive(arc_session_manager_->state())) {
    if (!stopped_callback.is_null())
      std::move(stopped_callback).Run(false /* success */);
    return;
  }

  arc_stopped_callback_ = std::move(stopped_callback);
  arc_session_manager_->AddObserver(this);

  // ARC is expected to be active and running here.
  arc_session_manager_->RequestDisable();
}

PrefService* ArcDataSnapshotdDelegate::GetProfilePrefService() {
  DCHECK(arc_session_manager_->profile());
  return arc_session_manager_->profile()->GetPrefs();
}

std::unique_ptr<ArcSnapshotRebootNotification>
ArcDataSnapshotdDelegate::CreateRebootNotification() {
  return std::make_unique<ArcSnapshotRebootNotificationImpl>();
}

std::unique_ptr<ArcAppsTracker> ArcDataSnapshotdDelegate::CreateAppsTracker() {
  return std::make_unique<ArcForceInstalledAppsTracker>();
}

void ArcDataSnapshotdDelegate::RestartChrome(
    const base::CommandLine& command_line) {
  ash::RestartChrome(command_line, ash::RestartChromeReason::kUserless);
}

void ArcDataSnapshotdDelegate::OnArcSessionStopped(arc::ArcStopReason reason) {
  switch (reason) {
    case arc::ArcStopReason::SHUTDOWN:
      // This shutdown is triggered only by RequestStopArcInstance.
      // It is guaranteed that user is not able to influence ArcEnabled pref.
      // If ARC is disabled by policy, the ARC snapshot feature gets disabled.
      DCHECK(IsArcStopped(arc_session_manager_->state()));
      NotifyArcStopped(true /* success */);
      return;
    case arc::ArcStopReason::GENERIC_BOOT_FAILURE:
    case arc::ArcStopReason::CRASH:
    case arc::ArcStopReason::LOW_DISK_SPACE:
      NotifyArcStopped(false /* success */);
      return;
  }
}

void ArcDataSnapshotdDelegate::NotifyArcStopped(bool success) {
  DCHECK(!arc_stopped_callback_.is_null());
  std::move(arc_stopped_callback_).Run(success);

  arc_session_manager_->RemoveObserver(this);
}

}  // namespace data_snapshotd
}  // namespace arc

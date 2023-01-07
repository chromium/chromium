// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/enterprise/snapshot_reboot_controller.h"

#include "base/logging.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"

namespace arc {
namespace data_snapshotd {

namespace {

// Requests reboot. The reboot may not happen immediately.
void RequestReboot() {
  chromeos::PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_OTHER, "ARC data snapshot");
}

// Returns true if any user is logged in.
bool IsUserLoggedIn() {
  return user_manager::UserManager::Get() &&
         user_manager::UserManager::Get()->IsUserLoggedIn();
}

}  // namespace

const int kMaxRebootAttempts = 3;
const base::TimeDelta kRebootAttemptDelay = base::Minutes(5);

SnapshotRebootController::SnapshotRebootController(
    std::unique_ptr<ArcSnapshotRebootNotification> notification)
    : notification_(std::move(notification)) {
  notification_->SetUserConsentCallback(
      base::BindRepeating(&SnapshotRebootController::HandleUserConsent,
                          weak_ptr_factory_.GetWeakPtr()));
  session_manager::SessionManager::Get()->AddObserver(this);
  if (IsUserLoggedIn()) {
    notification_->Show();
  } else {
    // The next operation after reboot is blocking, ensure no one uses device
    // during the next 5 mins.
    StartRebootTimer();
  }
}

SnapshotRebootController::~SnapshotRebootController() {
  session_manager::SessionManager::Get()->RemoveObserver(this);
}

void SnapshotRebootController::OnSessionStateChanged() {
  if (IsUserLoggedIn()) {
    StopRebootTimer();
    notification_->Show();
  } else {
    // The next operation after reboot is blocking, ensure no one uses device
    // during the next 5 mins.
    notification_->Hide();
    StartRebootTimer();
  }
}

void SnapshotRebootController::StartRebootTimer() {
  if (reboot_timer_.IsRunning())
    return;
  reboot_attempts_ = 0;
  SetRebootTimer();
}

void SnapshotRebootController::SetRebootTimer() {
  reboot_timer_.Start(FROM_HERE, kRebootAttemptDelay,
                      base::BindOnce(&SnapshotRebootController::OnRebootTimer,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void SnapshotRebootController::StopRebootTimer() {
  reboot_attempts_ = 0;
  if (!reboot_timer_.IsRunning())
    return;
  reboot_timer_.Stop();
}

void SnapshotRebootController::OnRebootTimer() {
  reboot_attempts_++;
  RequestReboot();
  if (reboot_attempts_ >= kMaxRebootAttempts) {
    LOG(ERROR) << "The number of reboot attempts exceeded for ARC snapshots.";
    return;
  }
  SetRebootTimer();
}

void SnapshotRebootController::HandleUserConsent() {
  RequestReboot();
  StartRebootTimer();
}

}  // namespace data_snapshotd
}  // namespace arc

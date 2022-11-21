// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/enterprise/arc_data_snapshotd_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/dbus/arc/arc_data_snapshotd_client.h"

namespace arc {
namespace data_snapshotd {

namespace {

// Interval between successful connection attempts.
constexpr base::TimeDelta kConnectionAttemptInterval = base::Seconds(1);

// The maximum number of consecutive connection attempts before giving up.
constexpr int kMaxConnectionAttemptCount = 5;

}  // namespace

ArcDataSnapshotdBridge::ArcDataSnapshotdBridge(
    base::OnceClosure on_bridge_available_callback)
    : on_bridge_available_callback_(std::move(on_bridge_available_callback)) {
  WaitForDBusService();
}

ArcDataSnapshotdBridge::~ArcDataSnapshotdBridge() = default;

// static
base::TimeDelta
ArcDataSnapshotdBridge::connection_attempt_interval_for_testing() {
  return kConnectionAttemptInterval;
}

// static
int ArcDataSnapshotdBridge::max_connection_attempt_count_for_testing() {
  return kMaxConnectionAttemptCount;
}

void ArcDataSnapshotdBridge::WaitForDBusService() {
  if (connection_attempt_ >= kMaxConnectionAttemptCount) {
    LOG(WARNING)
        << "Stopping attempts to connect to arc-data-snapshotd - too many "
           "unsuccessful attempts in a row";
    std::move(on_bridge_available_callback_).Run();
    return;
  }
  ++connection_attempt_;

  dbus_waiting_weak_ptr_factory_.InvalidateWeakPtrs();

  ash::ArcDataSnapshotdClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&ArcDataSnapshotdBridge::OnWaitedForDBusService,
                     dbus_waiting_weak_ptr_factory_.GetWeakPtr()));
  ScheduleWaitingForDBusService();
}

void ArcDataSnapshotdBridge::ScheduleWaitingForDBusService() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ArcDataSnapshotdBridge::WaitForDBusService,
                     dbus_waiting_weak_ptr_factory_.GetWeakPtr()),
      kConnectionAttemptInterval);
}

void ArcDataSnapshotdBridge::OnWaitedForDBusService(bool service_is_available) {
  if (!service_is_available) {
    LOG(WARNING) << "The arc-data-snapshotd D-Bus service is unavailable";
    return;
  }

  // Cancel any tasks previously created from WaitForDBusService() or
  // ScheduleWaitingForDBusService().
  dbus_waiting_weak_ptr_factory_.InvalidateWeakPtrs();
  is_available_ = true;
  std::move(on_bridge_available_callback_).Run();
}

void ArcDataSnapshotdBridge::GenerateKeyPair(
    base::OnceCallback<void(bool)> callback) {
  if (!is_available_) {
    LOG(ERROR) << "GenerateKeyPair call when D-Bus service is not available.";
    std::move(callback).Run(false /* success */);
    return;
  }

  VLOG(1) << "GenerateKeyPair via D-Bus";
  ash::ArcDataSnapshotdClient::Get()->GenerateKeyPair(std::move(callback));
}

void ArcDataSnapshotdBridge::ClearSnapshot(
    bool last,
    base::OnceCallback<void(bool)> callback) {
  if (!is_available_) {
    LOG(ERROR) << "ClearSnapshot call when D-Bus service is not available.";
    std::move(callback).Run(false /* success */);
    return;
  }
  VLOG(1) << "ClearSnapshot via D-Bus";
  ash::ArcDataSnapshotdClient::Get()->ClearSnapshot(last, std::move(callback));
}

void ArcDataSnapshotdBridge::TakeSnapshot(
    const std::string& account_id,
    base::OnceCallback<void(bool)> callback) {
  if (!is_available_) {
    LOG(ERROR) << "TakeSnapshot call when D-Bus service is not available.";
    std::move(callback).Run(false /* success */);
    return;
  }
  VLOG(1) << "TakeSnapshot via D-Bus";
  ash::ArcDataSnapshotdClient::Get()->TakeSnapshot(account_id,
                                                   std::move(callback));
}

void ArcDataSnapshotdBridge::LoadSnapshot(
    const std::string& account_id,
    base::OnceCallback<void(bool, bool)> callback) {
  if (!is_available_) {
    LOG(ERROR) << "LoadSnapshot call when D-Bus service is not available.";
    std::move(callback).Run(false /* success */, false /* last */);
    return;
  }
  VLOG(1) << "LoadSnapshot via D-Bus";
  ash::ArcDataSnapshotdClient::Get()->LoadSnapshot(account_id,
                                                   std::move(callback));
}

void ArcDataSnapshotdBridge::Update(int percent,
                                    base::OnceCallback<void(bool)> callback) {
  if (!is_available_) {
    LOG(ERROR) << "Update call when D-Bus service is not available.";
    std::move(callback).Run(false /* success */);
    return;
  }
  VLOG(1) << "Update via D-Bus";
  ash::ArcDataSnapshotdClient::Get()->Update(percent, std::move(callback));
}

void ArcDataSnapshotdBridge::ConnectToUiCancelledSignal(
    base::RepeatingClosure signal_callback) {
  if (!is_available_) {
    LOG(ERROR) << "Connection to UiCancelled signal when D-Bus service is not "
               << "available.";
    return;
  }
  VLOG(1) << "Connect to UiCancelled D-Bus signal.";
  ash::ArcDataSnapshotdClient::Get()->ConnectToUiCancelledSignal(
      std::move(signal_callback),
      base::BindOnce(
          &ArcDataSnapshotdBridge::OnUiCancelledSignalConnectedCallback,
          weak_ptr_factory_.GetWeakPtr()));
}

void ArcDataSnapshotdBridge::OnUiCancelledSignalConnectedCallback(
    bool success) {
  if (!success)
    LOG(ERROR) << "UiCancelled signal connection failed, will not cancel "
               << "snapshot generation from UI";
}

}  // namespace data_snapshotd
}  // namespace arc

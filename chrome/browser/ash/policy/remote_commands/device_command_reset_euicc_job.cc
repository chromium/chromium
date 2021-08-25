// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_reset_euicc_job.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/syslog_logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/cellular_utils.h"
#include "chromeos/network/network_handler.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

DeviceCommandResetEuiccJob::DeviceCommandResetEuiccJob()
    : DeviceCommandResetEuiccJob(
          chromeos::NetworkHandler::Get()->cellular_inhibitor()) {}

DeviceCommandResetEuiccJob::DeviceCommandResetEuiccJob(
    chromeos::CellularInhibitor* cellular_inhibitor)
    : cellular_inhibitor_(cellular_inhibitor) {}

DeviceCommandResetEuiccJob::~DeviceCommandResetEuiccJob() = default;

// static
std::unique_ptr<DeviceCommandResetEuiccJob>
DeviceCommandResetEuiccJob::CreateForTesting(
    chromeos::CellularInhibitor* cellular_inhibitor) {
  return base::WrapUnique(new DeviceCommandResetEuiccJob(cellular_inhibitor));
}

enterprise_management::RemoteCommand_Type DeviceCommandResetEuiccJob::GetType()
    const {
  return enterprise_management::RemoteCommand_Type_DEVICE_RESET_EUICC;
}

void DeviceCommandResetEuiccJob::RunImpl(CallbackWithResult succeeded_callback,
                                         CallbackWithResult failed_callback) {
  absl::optional<dbus::ObjectPath> euicc_path = chromeos::GetCurrentEuiccPath();
  if (!euicc_path) {
    SYSLOG(ERROR) << "No current EUICC. Unable to reset EUICC";
    RunResultCallback(std::move(failed_callback));
    return;
  }

  // TODO(crbug.com/1231305) Trigger a notification if an eSIM network is
  // active.
  SYSLOG(INFO) << "Executing EUICC reset memory remote command";
  cellular_inhibitor_->InhibitCellularScanning(
      chromeos::CellularInhibitor::InhibitReason::kResettingEuiccMemory,
      base::BindOnce(&DeviceCommandResetEuiccJob::PerformResetEuicc,
                     weak_ptr_factory_.GetWeakPtr(), *euicc_path,
                     std::move(succeeded_callback),
                     std::move(failed_callback)));
}

void DeviceCommandResetEuiccJob::PerformResetEuicc(
    dbus::ObjectPath euicc_path,
    CallbackWithResult succeeded_callback,
    CallbackWithResult failed_callback,
    std::unique_ptr<chromeos::CellularInhibitor::InhibitLock> inhibit_lock) {
  if (!inhibit_lock) {
    SYSLOG(ERROR) << "Unable to reset EUICC. Could not get inhibit lock";
    RunResultCallback(std::move(failed_callback));
    return;
  }

  chromeos::HermesEuiccClient::Get()->ResetMemory(
      euicc_path, hermes::euicc::ResetOptions::kDeleteOperationalProfiles,
      base::BindOnce(&DeviceCommandResetEuiccJob::OnResetMemoryResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(succeeded_callback), std::move(failed_callback),
                     std::move(inhibit_lock)));
}

void DeviceCommandResetEuiccJob::OnResetMemoryResponse(
    CallbackWithResult succeeded_callback,
    CallbackWithResult failed_callback,
    std::unique_ptr<chromeos::CellularInhibitor::InhibitLock> inhibit_lock,
    chromeos::HermesResponseStatus status) {
  if (status != chromeos::HermesResponseStatus::kSuccess) {
    SYSLOG(ERROR) << "Euicc reset failed. status = "
                  << static_cast<int>(status);
    RunResultCallback(std::move(failed_callback));
    return;
  }

  SYSLOG(INFO) << "Successfully cleared EUICC";
  RunResultCallback(std::move(succeeded_callback));

  // inhibit_lock is automatically released when destroyed.
}

void DeviceCommandResetEuiccJob::RunResultCallback(
    CallbackWithResult callback) {
  // Post |callback| to ensure async execution as required for RunImpl.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), /*result_payload=*/nullptr));
}

}  // namespace policy

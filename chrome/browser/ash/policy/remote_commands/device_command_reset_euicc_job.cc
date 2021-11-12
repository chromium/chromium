// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_reset_euicc_job.h"

#include <utility>

#include "ash/public/cpp/notification_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/syslog_logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/cellular_utils.h"
#include "chromeos/network/network_handler.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notification.h"

namespace policy {

namespace {

constexpr char kNotifierESimPolicy[] = "policy.esim-policy";

}  // namespace

// static
const char DeviceCommandResetEuiccJob::kResetEuiccNotificationId[] =
    "cros_reset_euicc";

void DeviceCommandResetEuiccJob::RecordResetEuiccResult(
    ResetEuiccResult result) {
  base::UmaHistogramEnumeration(
      "Network.Cellular.ESim.Policy.ResetEuicc.Result", result);
}

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

DeviceCommandResetEuiccJob::CallbackWithResult
DeviceCommandResetEuiccJob::CreateTimedResetMemorySuccessCallback(
    CallbackWithResult success_callback) {
  return base::BindOnce(
      [](CallbackWithResult success_callback, base::Time reset_euicc_start_time,
         std::unique_ptr<ResultPayload> result_pay_load) -> void {
        std::move(success_callback).Run(std::move(result_pay_load));
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Network.Cellular.ESim.Policy.ResetEuicc.Duration",
            base::Time::Now() - reset_euicc_start_time);
      },
      std::move(success_callback), base::Time::Now());
}

void DeviceCommandResetEuiccJob::RunImpl(CallbackWithResult succeeded_callback,
                                         CallbackWithResult failed_callback) {
  absl::optional<dbus::ObjectPath> euicc_path = chromeos::GetCurrentEuiccPath();
  if (!euicc_path) {
    SYSLOG(ERROR) << "No current EUICC. Unable to reset EUICC";
    RunResultCallback(std::move(failed_callback));
    return;
  }

  ShowResetEuiccNotification();
  SYSLOG(INFO) << "Executing EUICC reset memory remote command";
  cellular_inhibitor_->InhibitCellularScanning(
      chromeos::CellularInhibitor::InhibitReason::kResettingEuiccMemory,
      base::BindOnce(
          &DeviceCommandResetEuiccJob::PerformResetEuicc,
          weak_ptr_factory_.GetWeakPtr(), *euicc_path,
          CreateTimedResetMemorySuccessCallback(std::move(succeeded_callback)),
          std::move(failed_callback)));
}

void DeviceCommandResetEuiccJob::PerformResetEuicc(
    dbus::ObjectPath euicc_path,
    CallbackWithResult succeeded_callback,
    CallbackWithResult failed_callback,
    std::unique_ptr<chromeos::CellularInhibitor::InhibitLock> inhibit_lock) {
  if (!inhibit_lock) {
    SYSLOG(ERROR) << "Unable to reset EUICC. Could not get inhibit lock";
    RecordResetEuiccResult(ResetEuiccResult::kInhibitFailed);
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
    RecordResetEuiccResult(ResetEuiccResult::kHermesResetFailed);
    RunResultCallback(std::move(failed_callback));
    return;
  }

  SYSLOG(INFO) << "Successfully cleared EUICC";
  RecordResetEuiccResult(ResetEuiccResult::kSuccess);
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

void DeviceCommandResetEuiccJob::ShowResetEuiccNotification() {
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kResetEuiccNotificationId,
          l10n_util::GetStringUTF16(
              IDS_ASH_NETWORK_RESET_EUICC_NOTIFICATION_TITLE),
          l10n_util::GetStringUTF16(
              IDS_ASH_NETWORK_RESET_EUICC_NOTIFICATION_MESSAGE),
          /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierESimPolicy),
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::DoNothingAs<void()>()),
          /*small_image=*/gfx::VectorIcon(),
          message_center::SystemNotificationWarningLevel::NORMAL);
  SystemNotificationHelper::GetInstance()->Display(*notification);
}

}  // namespace policy

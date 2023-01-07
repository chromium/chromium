// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_reset_euicc_job.h"

#include <utility>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/syslog_logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chromeos/ash/components/network/cellular_esim_uninstall_handler.h"
#include "chromeos/ash/components/network/cellular_utils.h"
#include "chromeos/ash/components/network/network_handler.h"
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

DeviceCommandResetEuiccJob::DeviceCommandResetEuiccJob() = default;
DeviceCommandResetEuiccJob::~DeviceCommandResetEuiccJob() = default;

enterprise_management::RemoteCommand_Type DeviceCommandResetEuiccJob::GetType()
    const {
  return enterprise_management::RemoteCommand_Type_DEVICE_RESET_EUICC;
}

DeviceCommandResetEuiccJob::CallbackWithResult
DeviceCommandResetEuiccJob::CreateTimedResetMemorySuccessCallback(
    CallbackWithResult success_callback) {
  return base::BindOnce(
      [](CallbackWithResult success_callback, base::Time reset_euicc_start_time,
         absl::optional<std::string> result_payload) {
        std::move(success_callback).Run(std::move(result_payload));
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Network.Cellular.ESim.Policy.ResetEuicc.Duration",
            base::Time::Now() - reset_euicc_start_time);
      },
      std::move(success_callback), base::Time::Now());
}

void DeviceCommandResetEuiccJob::RunImpl(CallbackWithResult succeeded_callback,
                                         CallbackWithResult failed_callback) {
  absl::optional<dbus::ObjectPath> euicc_path = ash::GetCurrentEuiccPath();
  if (!euicc_path) {
    SYSLOG(ERROR) << "No current EUICC. Unable to reset EUICC";
    RunResultCallback(std::move(failed_callback));
    return;
  }

  SYSLOG(INFO) << "Executing EUICC reset memory remote command";
  ash::CellularESimUninstallHandler* uninstall_handler =
      ash::NetworkHandler::Get()->cellular_esim_uninstall_handler();
  uninstall_handler->ResetEuiccMemory(
      *euicc_path,
      base::BindOnce(
          &DeviceCommandResetEuiccJob::OnResetMemoryResponse,
          weak_ptr_factory_.GetWeakPtr(),
          CreateTimedResetMemorySuccessCallback(std::move(succeeded_callback)),
          std::move(failed_callback)));
}

void DeviceCommandResetEuiccJob::OnResetMemoryResponse(
    CallbackWithResult succeeded_callback,
    CallbackWithResult failed_callback,
    bool success) {
  if (!success) {
    SYSLOG(ERROR) << "Euicc reset failed.";
    RecordResetEuiccResult(ResetEuiccResult::kHermesResetFailed);
    RunResultCallback(std::move(failed_callback));
    return;
  }

  SYSLOG(INFO) << "Successfully cleared EUICC";
  RecordResetEuiccResult(ResetEuiccResult::kSuccess);
  RunResultCallback(std::move(succeeded_callback));
  ShowResetEuiccNotification();
}

void DeviceCommandResetEuiccJob::RunResultCallback(
    CallbackWithResult callback) {
  // Post |callback| to ensure async execution as required for RunImpl.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), /*result_payload=*/absl::nullopt));
}

void DeviceCommandResetEuiccJob::ShowResetEuiccNotification() {
  message_center::Notification notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kResetEuiccNotificationId,
      l10n_util::GetStringUTF16(IDS_ASH_NETWORK_RESET_EUICC_NOTIFICATION_TITLE),
      l10n_util::GetStringUTF16(
          IDS_ASH_NETWORK_RESET_EUICC_NOTIFICATION_MESSAGE),
      /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT, kNotifierESimPolicy,
          ash::NotificationCatalogName::kDeviceCommandReset),
      message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::DoNothingAs<void()>()),
      /*small_image=*/gfx::VectorIcon(),
      message_center::SystemNotificationWarningLevel::NORMAL);
  SystemNotificationHelper::GetInstance()->Display(notification);
}

}  // namespace policy

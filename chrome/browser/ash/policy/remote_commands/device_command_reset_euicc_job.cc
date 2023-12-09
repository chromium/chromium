// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_reset_euicc_job.h"

#include <optional>
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
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notification.h"

namespace policy {

namespace {

constexpr char kNotifierESimPolicy[] = "policy.esim-policy";

// The timeout is increased as per b/293583300.
constexpr base::TimeDelta kEuiccCommandExpirationTime = base::Days(180);

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

bool DeviceCommandResetEuiccJob::IsExpired(base::TimeTicks now) {
  return now > issued_time() + kEuiccCommandExpirationTime;
}

void DeviceCommandResetEuiccJob::RunImpl(CallbackWithResult result_callback) {
  std::optional<dbus::ObjectPath> euicc_path =
      ash::cellular_utils::GetCurrentEuiccPath();
  if (!euicc_path) {
    SYSLOG(ERROR) << "No current EUICC. Unable to reset EUICC";
    RunResultCallback(std::move(result_callback), ResultType::kFailure);
    return;
  }

  SYSLOG(INFO) << "Executing EUICC reset memory remote command";
  ash::CellularESimUninstallHandler* uninstall_handler =
      ash::NetworkHandler::Get()->cellular_esim_uninstall_handler();
  uninstall_handler->ResetEuiccMemory(
      *euicc_path,
      base::BindOnce(&DeviceCommandResetEuiccJob::OnResetMemoryResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result_callback),
                     base::Time::Now()));
}

void DeviceCommandResetEuiccJob::OnResetMemoryResponse(
    CallbackWithResult result_callback,
    base::Time reset_euicc_start_time,
    bool success) {
  if (!success) {
    SYSLOG(ERROR) << "Euicc reset failed.";
    RecordResetEuiccResult(ResetEuiccResult::kHermesResetFailed);
    RunResultCallback(std::move(result_callback), ResultType::kFailure);
    return;
  }

  SYSLOG(INFO) << "Successfully cleared EUICC";
  RecordResetEuiccResult(ResetEuiccResult::kSuccess);
  RunResultCallback(std::move(result_callback), ResultType::kSuccess);
  UMA_HISTOGRAM_MEDIUM_TIMES("Network.Cellular.ESim.Policy.ResetEuicc.Duration",
                             base::Time::Now() - reset_euicc_start_time);
  ShowResetEuiccNotification();
}

void DeviceCommandResetEuiccJob::RunResultCallback(CallbackWithResult callback,
                                                   ResultType result) {
  // Post |callback| to ensure async execution as required for RunImpl.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result,
                                /*result_payload=*/std::nullopt));
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

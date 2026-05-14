// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_query_geolocation_job.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chromeos/ash/components/geolocation/system_location_provider.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace policy {

namespace {
constexpr char kResultCode[] = "result_code";
constexpr char kLatitude[] = "latitude";
constexpr char kLongitude[] = "longitude";
constexpr char kAccuracy[] = "accuracy";
constexpr char kQueryTimeMs[] = "query_time_ms";
constexpr char kErrorMessage[] = "error_message";
constexpr char kErrorCode[] = "error_code";
constexpr base::TimeDelta kGeolocationTimeout = base::Seconds(60);
constexpr base::TimeDelta kRetryDelay = base::Seconds(10);

constexpr char kLocationSavedNotificationId[] =
    "device-located-disabled-device";

class LocationSavedNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  LocationSavedNotificationDelegate() = default;

  void Close(bool by_user) override {
    if (by_user) {
      g_browser_process->local_state()->ClearPref(
          ash::prefs::kDeviceCommandQueryGeolocationReported);
    }
  }

 private:
  ~LocationSavedNotificationDelegate() override = default;
};

}  // namespace

DeviceCommandQueryGeolocationJob::DeviceCommandQueryGeolocationJob(
    const DeviceCloudPolicyManagerAsh* policy_manager)
    : policy_manager_(policy_manager) {}

DeviceCommandQueryGeolocationJob::~DeviceCommandQueryGeolocationJob() = default;

// static
void DeviceCommandQueryGeolocationJob::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      ash::prefs::kDeviceCommandQueryGeolocationReported, false);
}

// static
void DeviceCommandQueryGeolocationJob::
    ShowLocationReportedNotificationIfNeeded() {
  if (!g_browser_process->local_state()->GetBoolean(
          ash::prefs::kDeviceCommandQueryGeolocationReported)) {
    return;
  }

  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kLocationSavedNotificationId,
      ash::NotificationCatalogName::kDeviceCommandGeolocation);

  message_center::RichNotificationData notification_data;
  notification_data.priority = message_center::SYSTEM_PRIORITY;
  notification_data.never_timeout = true;

  auto notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kLocationSavedNotificationId,
      l10n_util::GetStringUTF16(IDS_POLICY_DEVICE_LOCATED_TITLE),
      l10n_util::GetStringUTF16(IDS_POLICY_DEVICE_LOCATED_MESSAGE),
      /*display_source=*/std::u16string(), /*origin_url=*/GURL(), notifier_id,
      notification_data,
      base::MakeRefCounted<LocationSavedNotificationDelegate>(),
      vector_icons::kBusinessOldIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);

  SystemNotificationHelper::GetInstance()->Display(notification);
}

enterprise_management::RemoteCommand_Type
DeviceCommandQueryGeolocationJob::GetType() const {
  return enterprise_management::RemoteCommand_Type_QUERY_GEOLOCATION;
}

std::optional<enterprise_management::QueryGeolocationCommandResultCode>
DeviceCommandQueryGeolocationJob::CheckIfCommandIsAllowed() const {
  if (!policy_manager_ || !policy_manager_->core()->store()) {
    return enterprise_management::QueryGeolocationCommandResultCode::
        DEVICE_NOT_MANAGED;
  }

  const enterprise_management::PolicyData* policy =
      policy_manager_->core()->store()->policy();
  if (!policy || !policy->has_device_state()) {
    return enterprise_management::QueryGeolocationCommandResultCode::
        DEVICE_NOT_MANAGED;
  }

  const enterprise_management::DeviceState& device_state =
      policy->device_state();

  if (device_state.device_mode() !=
      enterprise_management::DeviceState::DEVICE_MODE_DISABLED) {
    return enterprise_management::QueryGeolocationCommandResultCode::
        DEVICE_NOT_DISABLED;
  }

  if (!device_state.has_disabled_state() ||
      !device_state.disabled_state().location_tracking_enabled()) {
    return enterprise_management::QueryGeolocationCommandResultCode::
        LOCATION_TRACKING_DISABLED;
  }
  return std::nullopt;
}

void DeviceCommandQueryGeolocationJob::RunImpl(
    CallbackWithResult result_callback) {
  RunImplInternal(std::move(result_callback), /*retried=*/false);
}

void DeviceCommandQueryGeolocationJob::RunImplInternal(
    CallbackWithResult result_callback,
    bool retried) {
  const std::optional<enterprise_management::QueryGeolocationCommandResultCode>
      error = CheckIfCommandIsAllowed();
  if (error.has_value()) {
    if (!retried) {
      // Retry once since the device might not have received the updated
      // disabled state yet.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&DeviceCommandQueryGeolocationJob::RunImplInternal,
                         weak_factory_.GetWeakPtr(),
                         std::move(result_callback),
                         /*retried=*/true),
          kRetryDelay);
      return;
    }

    base::DictValue error_response;
    error_response.Set(kResultCode, error.value());

    std::string payload;
    base::JSONWriter::Write(error_response, &payload);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback),
                                  ResultType::kFailure, std::move(payload)));
    return;
  }

  ash::SystemLocationProvider::GetInstance()->RequestGeolocation(
      kGeolocationTimeout, /*send_wifi_access_points=*/true,
      /*send_cell_towers=*/true,
      base::BindOnce(&DeviceCommandQueryGeolocationJob::OnLocationResponse,
                     weak_factory_.GetWeakPtr(), std::move(result_callback)),
      ash::SystemLocationProvider::ClientId::kGeolocationController);
}

void DeviceCommandQueryGeolocationJob::OnLocationResponse(
    CallbackWithResult result_callback,
    const ash::Geoposition& position,
    bool server_error,
    const base::TimeDelta elapsed) {
  base::DictValue dict;
  ResultType result_type = ResultType::kFailure;
  if (position.Valid()) {
    result_type = ResultType::kSuccess;
    dict.Set(kResultCode,
             enterprise_management::QueryGeolocationCommandResultCode::SUCCESS);
    dict.Set(kLatitude, position.latitude);
    dict.Set(kLongitude, position.longitude);
    dict.Set(kAccuracy, position.accuracy);
    dict.Set(kQueryTimeMs,
             base::NumberToString(
                 position.timestamp.InMillisecondsSinceUnixEpoch()));

    g_browser_process->local_state()->SetBoolean(
        ash::prefs::kDeviceCommandQueryGeolocationReported, true);
  } else {
    if (!position.error_message.empty()) {
      dict.Set(kErrorMessage, position.error_message);
    }
    if (position.error_code != 0) {
      dict.Set(kErrorCode, position.error_code);
    }

    enterprise_management::QueryGeolocationCommandResultCode result_code;
    switch (position.status) {
      case ash::Geoposition::STATUS_TIMEOUT:
        result_code =
            enterprise_management::QueryGeolocationCommandResultCode::TIMEOUT;
        break;
      case ash::Geoposition::STATUS_SERVER_ERROR:
        result_code = enterprise_management::QueryGeolocationCommandResultCode::
            SERVER_ERROR;
        break;
      case ash::Geoposition::STATUS_NETWORK_ERROR:
        result_code = enterprise_management::QueryGeolocationCommandResultCode::
            NETWORK_ERROR;
        break;
      case ash::Geoposition::STATUS_OK:
      case ash::Geoposition::STATUS_NONE:
        NOTREACHED();
    }
    dict.Set(kResultCode, result_code);
  }

  std::string payload;
  base::JSONWriter::Write(dict, &payload);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(result_callback), result_type,
                                std::move(payload)));
}

}  // namespace policy

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_query_geolocation_job.h"

#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chromeos/ash/components/geolocation/system_location_provider.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/policy/proto/device_management_backend.pb.h"

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
}  // namespace

DeviceCommandQueryGeolocationJob::DeviceCommandQueryGeolocationJob(
    const DeviceCloudPolicyManagerAsh* policy_manager)
    : policy_manager_(policy_manager) {}

DeviceCommandQueryGeolocationJob::~DeviceCommandQueryGeolocationJob() = default;

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
  const std::optional<enterprise_management::QueryGeolocationCommandResultCode>
      error = CheckIfCommandIsAllowed();
  if (error.has_value()) {
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

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/multidevice_debug/proximity_auth_webui_handler.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <utility>

#include "base/base64url.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/values.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"
#include "chromeos/ash/services/device_sync/proto/enum_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace ash {

namespace multidevice {

namespace {

constexpr const multidevice::SoftwareFeature kAllSoftareFeatures[] = {
    multidevice::SoftwareFeature::kBetterTogetherHost,
    multidevice::SoftwareFeature::kBetterTogetherClient,
    multidevice::SoftwareFeature::kSmartLockHost,
    multidevice::SoftwareFeature::kSmartLockClient,
    multidevice::SoftwareFeature::kInstantTetheringHost,
    multidevice::SoftwareFeature::kInstantTetheringClient,
    multidevice::SoftwareFeature::kMessagesForWebHost,
    multidevice::SoftwareFeature::kMessagesForWebClient,
    multidevice::SoftwareFeature::kPhoneHubHost,
    multidevice::SoftwareFeature::kPhoneHubClient,
    multidevice::SoftwareFeature::kWifiSyncHost,
    multidevice::SoftwareFeature::kWifiSyncClient,
    multidevice::SoftwareFeature::kEcheHost,
    multidevice::SoftwareFeature::kEcheClient,
    multidevice::SoftwareFeature::kPhoneHubCameraRollHost,
    multidevice::SoftwareFeature::kPhoneHubCameraRollClient};

// Keys in the JSON representation of a log message.
const char kLogMessageTextKey[] = "text";
const char kLogMessageTimeKey[] = "time";
const char kLogMessageFileKey[] = "file";
const char kLogMessageLineKey[] = "line";
const char kLogMessageSeverityKey[] = "severity";

// Keys in the JSON representation of a SyncState object for enrollment or
// device sync.
const char kSyncStateLastSuccessTime[] = "lastSuccessTime";
const char kSyncStateNextRefreshTime[] = "nextRefreshTime";
const char kSyncStateRecoveringFromFailure[] = "recoveringFromFailure";
const char kSyncStateOperationInProgress[] = "operationInProgress";

// 9999 days in milliseconds.
const double kFakeInfinityMillis = 863913600000;

double ConvertNextAttemptTimeToDouble(base::TimeDelta delta) {
  // If no future attempt is scheduled, the next-attempt time is
  // base::TimeDelta::Max(), which corresponds to an infinite double value. In
  // order to store the next-attempt time as a double base::Value,
  // std::isfinite() must be true. So, here we use 9999 days to represent the
  // max next-attempt time to allow use with base::Value.
  if (delta.is_max())
    return kFakeInfinityMillis;

  return delta.InMillisecondsF();
}

// Converts |log_message| to a raw dictionary value used as a JSON argument to
// JavaScript functions.
base::Value::Dict LogMessageToDictionary(
    const multidevice::LogBuffer::LogMessage& log_message) {
  base::Value::Dict dictionary;
  dictionary.Set(kLogMessageTextKey, log_message.text);
  dictionary.Set(kLogMessageTimeKey,
                 base::TimeFormatTimeOfDayWithMilliseconds(log_message.time));
  dictionary.Set(kLogMessageFileKey, log_message.file);
  dictionary.Set(kLogMessageLineKey, log_message.line);
  dictionary.Set(kLogMessageSeverityKey,
                 static_cast<int>(log_message.severity));
  return dictionary;
}

// Keys in the JSON representation of an ExternalDeviceInfo proto.
const char kExternalDevicePublicKey[] = "publicKey";
const char kExternalDevicePublicKeyTruncated[] = "publicKeyTruncated";
const char kExternalDeviceFriendlyName[] = "friendlyDeviceName";
const char kExternalDeviceNoPiiName[] = "noPiiName";
const char kExternalDeviceUnlockKey[] = "unlockKey";
const char kExternalDeviceMobileHotspot[] = "hasMobileHotspot";
const char kExternalDeviceFeatureStates[] = "featureStates";

// Creates a SyncState JSON object that can be passed to the WebUI.
base::Value::Dict CreateSyncStateDictionary(double last_success_time,
                                            double next_refresh_time,
                                            bool is_recovering_from_failure,
                                            bool is_enrollment_in_progress) {
  base::Value::Dict sync_state;
  sync_state.Set(kSyncStateLastSuccessTime, last_success_time);
  sync_state.Set(kSyncStateNextRefreshTime, next_refresh_time);
  sync_state.Set(kSyncStateRecoveringFromFailure, is_recovering_from_failure);
  sync_state.Set(kSyncStateOperationInProgress, is_enrollment_in_progress);
  return sync_state;
}

std::string GenerateFeaturesString(const multidevice::RemoteDeviceRef& device) {
  std::stringstream ss;
  ss << "{";

  bool logged_feature = false;
  for (const auto& software_feature : kAllSoftareFeatures) {
    multidevice::SoftwareFeatureState state =
        device.GetSoftwareFeatureState(software_feature);

    // Only log features with values.
    if (state == multidevice::SoftwareFeatureState::kNotSupported)
      continue;

    logged_feature = true;
    ss << software_feature << ": " << state << ", ";
  }

  if (logged_feature)
    ss.seekp(-2, ss.cur);  // Remove last ", " from the stream.

  ss << "}";
  return ss.str();
}

}  // namespace

ProximityAuthWebUIHandler::ProximityAuthWebUIHandler(
    device_sync::DeviceSyncClient* device_sync_client)
    : device_sync_client_(device_sync_client),
      web_contents_initialized_(false) {
  CHECK(device_sync_client_);
}

ProximityAuthWebUIHandler::~ProximityAuthWebUIHandler() {
  multidevice::LogBuffer::GetInstance()->RemoveObserver(this);

  device_sync_client_->RemoveObserver(this);
}

void ProximityAuthWebUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "onWebContentsInitialized",
      base::BindRepeating(&ProximityAuthWebUIHandler::OnWebContentsInitialized,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "clearLogBuffer",
      base::BindRepeating(&ProximityAuthWebUIHandler::ClearLogBuffer,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getLogMessages",
      base::BindRepeating(&ProximityAuthWebUIHandler::GetLogMessages,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getLocalState",
      base::BindRepeating(&ProximityAuthWebUIHandler::GetLocalState,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "forceEnrollment",
      base::BindRepeating(&ProximityAuthWebUIHandler::ForceEnrollment,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "forceDeviceSync",
      base::BindRepeating(&ProximityAuthWebUIHandler::ForceDeviceSync,
                          base::Unretained(this)));
}

void ProximityAuthWebUIHandler::OnLogMessageAdded(
    const multidevice::LogBuffer::LogMessage& log_message) {
  web_ui()->CallJavascriptFunctionUnsafe("LogBufferInterface.onLogMessageAdded",
                                         LogMessageToDictionary(log_message));
}

void ProximityAuthWebUIHandler::OnLogBufferCleared() {
  web_ui()->CallJavascriptFunctionUnsafe(
      "LogBufferInterface.onLogBufferCleared");
}

void ProximityAuthWebUIHandler::OnEnrollmentFinished() {
  // OnGetDebugInfo() will call NotifyOnEnrollmentFinished() with the enrollment
  // state info.
  enrollment_update_waiting_for_debug_info_ = true;
  device_sync_client_->GetDebugInfo(
      base::BindOnce(&ProximityAuthWebUIHandler::OnGetDebugInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProximityAuthWebUIHandler::OnNewDevicesSynced() {
  // OnGetDebugInfo() will call NotifyOnSyncFinished() with the device sync
  // state info.
  sync_update_waiting_for_debug_info_ = true;
  device_sync_client_->GetDebugInfo(
      base::BindOnce(&ProximityAuthWebUIHandler::OnGetDebugInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProximityAuthWebUIHandler::OnWebContentsInitialized(
    const base::Value::List& args) {
  if (!web_contents_initialized_) {
    device_sync_client_->AddObserver(this);
    multidevice::LogBuffer::GetInstance()->AddObserver(this);
    web_contents_initialized_ = true;
  }
}

void ProximityAuthWebUIHandler::GetLogMessages(const base::Value::List& args) {
  base::Value::List json_logs;
  for (const auto& log : *multidevice::LogBuffer::GetInstance()->logs()) {
    json_logs.Append(LogMessageToDictionary(log));
  }
  web_ui()->CallJavascriptFunctionUnsafe("LogBufferInterface.onGotLogMessages",
                                         json_logs);
}

void ProximityAuthWebUIHandler::ClearLogBuffer(const base::Value::List& args) {
  // The OnLogBufferCleared() observer function will be called after the buffer
  // is cleared.
  multidevice::LogBuffer::GetInstance()->Clear();
}

void ProximityAuthWebUIHandler::ForceEnrollment(const base::Value::List& args) {
  device_sync_client_->ForceEnrollmentNow(
      base::BindOnce(&ProximityAuthWebUIHandler::OnForceEnrollmentNow,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProximityAuthWebUIHandler::ForceDeviceSync(const base::Value::List& args) {
  device_sync_client_->ForceSyncNow(
      base::BindOnce(&ProximityAuthWebUIHandler::OnForceSyncNow,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProximityAuthWebUIHandler::GetLocalState(const base::Value::List& args) {
  // OnGetDebugInfo() will call NotifyGotLocalState() with the enrollment and
  // device sync state info.
  get_local_state_update_waiting_for_debug_info_ = true;
  device_sync_client_->GetDebugInfo(
      base::BindOnce(&ProximityAuthWebUIHandler::OnGetDebugInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

base::Value ProximityAuthWebUIHandler::GetTruncatedLocalDeviceId() {
  std::optional<multidevice::RemoteDeviceRef> local_device_metadata =
      device_sync_client_->GetLocalDeviceMetadata();

  std::string device_id =
      local_device_metadata
          ? local_device_metadata->GetTruncatedDeviceIdForLogs()
          : "Missing Device ID";

  return base::Value(device_id);
}

base::Value::List ProximityAuthWebUIHandler::GetRemoteDevicesList() {
  base::Value::List devices_list_value;

  for (const auto& remote_device : device_sync_client_->GetSyncedDevices()) {
    devices_list_value.Append(RemoteDeviceToDictionary(remote_device));
  }

  return devices_list_value;
}

base::Value::Dict ProximityAuthWebUIHandler::RemoteDeviceToDictionary(
    const multidevice::RemoteDeviceRef& remote_device) {
  // Set the fields in the ExternalDeviceInfo proto.
  base::Value::Dict dictionary;
  dictionary.Set(kExternalDevicePublicKey, remote_device.GetDeviceId());
  dictionary.Set(kExternalDevicePublicKeyTruncated,
                 remote_device.GetTruncatedDeviceIdForLogs());
  dictionary.Set(kExternalDeviceFriendlyName, remote_device.name());
  dictionary.Set(kExternalDeviceNoPiiName, remote_device.pii_free_name());
  dictionary.Set(kExternalDeviceUnlockKey,
                 remote_device.GetSoftwareFeatureState(
                     multidevice::SoftwareFeature::kSmartLockHost) ==
                     multidevice::SoftwareFeatureState::kEnabled);
  dictionary.Set(kExternalDeviceMobileHotspot,
                 remote_device.GetSoftwareFeatureState(
                     multidevice::SoftwareFeature::kInstantTetheringHost) ==
                     multidevice::SoftwareFeatureState::kSupported);
  dictionary.Set(kExternalDeviceFeatureStates,
                 GenerateFeaturesString(remote_device));

  return dictionary;
}

void ProximityAuthWebUIHandler::OnForceEnrollmentNow(bool success) {
  PA_LOG(VERBOSE) << "Force enrollment result: " << success;
}

void ProximityAuthWebUIHandler::OnForceSyncNow(bool success) {
  PA_LOG(VERBOSE) << "Force sync result: " << success;
}

void ProximityAuthWebUIHandler::OnSetSoftwareFeatureState(
    const std::string public_key,
    device_sync::mojom::NetworkRequestResult result_code) {
  std::string device_id = RemoteDevice::GenerateDeviceId(public_key);

  if (result_code == device_sync::mojom::NetworkRequestResult::kSuccess) {
    PA_LOG(VERBOSE) << "Successfully set SoftwareFeature state for device: "
                    << device_id;
  } else {
    PA_LOG(ERROR) << "Failed to set SoftwareFeature state for device: "
                  << device_id << ", error code: " << result_code;
  }
}

void ProximityAuthWebUIHandler::OnGetDebugInfo(
    device_sync::mojom::DebugInfoPtr debug_info_ptr) {
  // If enrollment is not yet complete, no debug information is available.
  if (!debug_info_ptr)
    return;

  if (enrollment_update_waiting_for_debug_info_) {
    enrollment_update_waiting_for_debug_info_ = false;
    NotifyOnEnrollmentFinished(
        true /* success */,
        CreateSyncStateDictionary(
            debug_info_ptr->last_enrollment_time
                .InMillisecondsFSinceUnixEpoch(),
            ConvertNextAttemptTimeToDouble(
                debug_info_ptr->time_to_next_enrollment_attempt),
            debug_info_ptr->is_recovering_from_enrollment_failure,
            debug_info_ptr->is_enrollment_in_progress));
  }

  if (sync_update_waiting_for_debug_info_) {
    sync_update_waiting_for_debug_info_ = false;
    NotifyOnSyncFinished(
        true /* was_sync_successful */, true /* changed */,
        CreateSyncStateDictionary(
            debug_info_ptr->last_sync_time.InMillisecondsFSinceUnixEpoch(),
            ConvertNextAttemptTimeToDouble(
                debug_info_ptr->time_to_next_sync_attempt),
            debug_info_ptr->is_recovering_from_sync_failure,
            debug_info_ptr->is_sync_in_progress));
  }

  if (get_local_state_update_waiting_for_debug_info_) {
    get_local_state_update_waiting_for_debug_info_ = false;
    NotifyGotLocalState(
        GetTruncatedLocalDeviceId(),
        CreateSyncStateDictionary(
            debug_info_ptr->last_enrollment_time
                .InMillisecondsFSinceUnixEpoch(),
            ConvertNextAttemptTimeToDouble(
                debug_info_ptr->time_to_next_enrollment_attempt),
            debug_info_ptr->is_recovering_from_enrollment_failure,
            debug_info_ptr->is_enrollment_in_progress),
        CreateSyncStateDictionary(
            debug_info_ptr->last_sync_time.InMillisecondsFSinceUnixEpoch(),
            ConvertNextAttemptTimeToDouble(
                debug_info_ptr->time_to_next_sync_attempt),
            debug_info_ptr->is_recovering_from_sync_failure,
            debug_info_ptr->is_sync_in_progress),
        GetRemoteDevicesList());
  }
}

void ProximityAuthWebUIHandler::NotifyOnEnrollmentFinished(
    bool success,
    base::Value::Dict enrollment_state) {
  PA_LOG(VERBOSE) << "Enrollment attempt completed with success=" << success
                  << ":\n"
                  << enrollment_state;
  web_ui()->CallJavascriptFunctionUnsafe(
      "LocalStateInterface.onEnrollmentStateChanged", enrollment_state);
}

void ProximityAuthWebUIHandler::NotifyOnSyncFinished(
    bool was_sync_successful,
    bool changed,
    base::Value::Dict device_sync_state) {
  PA_LOG(VERBOSE) << "Device sync completed with result=" << was_sync_successful
                  << ":\n"
                  << device_sync_state;
  web_ui()->CallJavascriptFunctionUnsafe(
      "LocalStateInterface.onDeviceSyncStateChanged", device_sync_state);

  if (changed) {
    base::Value::List synced_devices = GetRemoteDevicesList();
    PA_LOG(VERBOSE) << "New unlock keys obtained after device sync:\n"
                    << synced_devices;
    web_ui()->CallJavascriptFunctionUnsafe(
        "LocalStateInterface.onRemoteDevicesChanged", synced_devices);
  }
}

void ProximityAuthWebUIHandler::NotifyGotLocalState(
    base::Value truncated_local_device_id,
    base::Value::Dict enrollment_state,
    base::Value::Dict device_sync_state,
    base::Value::List synced_devices) {
  PA_LOG(VERBOSE) << "==== Got Local State ====\n"
                  << "Device ID (truncated): " << truncated_local_device_id
                  << "\nEnrollment State: \n"
                  << enrollment_state << "Device Sync State: \n"
                  << device_sync_state << "Synced devices: \n"
                  << synced_devices;
  web_ui()->CallJavascriptFunctionUnsafe(
      "LocalStateInterface.onGotLocalState", truncated_local_device_id,
      enrollment_state, device_sync_state, synced_devices);
}

}  // namespace multidevice

}  // namespace ash

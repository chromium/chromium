// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/device_reporting_settings_lacros.h"

#include <memory>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece_forward.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "components/policy/policy_constants.h"

namespace reporting {
namespace {

bool IsSupportedPolicy(base::StringPiece policy_name) {
  return policy_name == ::policy::key::kReportDeviceNetworkStatus ||
         policy_name == ::policy::key::kReportUploadFrequency ||
         policy_name ==
             ::policy::key::kReportDeviceNetworkTelemetryCollectionRateMs;
}

}  // namespace

void DeviceReportingSettingsLacros::Delegate::RegisterObserverWithCrosApiClient(
    DeviceReportingSettingsLacros* const instance) {
  g_browser_process->browser_policy_connector()
      ->device_settings_lacros()
      ->AddObserver(instance);
}

crosapi::mojom::DeviceSettings*
DeviceReportingSettingsLacros::Delegate::GetDeviceSettings() {
  return g_browser_process->browser_policy_connector()->GetDeviceSettings();
}

// static
std::unique_ptr<DeviceReportingSettingsLacros>
DeviceReportingSettingsLacros::Create() {
  auto delegate = std::make_unique<DeviceReportingSettingsLacros::Delegate>();
  return base::WrapUnique(
      new DeviceReportingSettingsLacros(std::move(delegate)));
}

// static
std::unique_ptr<DeviceReportingSettingsLacros>
DeviceReportingSettingsLacros::CreateForTest(
    std::unique_ptr<DeviceReportingSettingsLacros::Delegate> delegate) {
  return base::WrapUnique(
      new DeviceReportingSettingsLacros(std::move(delegate)));
}

DeviceReportingSettingsLacros::DeviceReportingSettingsLacros(
    std::unique_ptr<DeviceReportingSettingsLacros::Delegate> delegate)
    : delegate_(std::move(delegate)) {
  delegate_->RegisterObserverWithCrosApiClient(this);
}

DeviceReportingSettingsLacros::~DeviceReportingSettingsLacros() = default;

void DeviceReportingSettingsLacros::OnDeviceSettingsUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Notify relevant observers only if the corresponding device setting value
  // has changed.
  for (const auto& observer : observer_map_) {
    const std::string& policy_name = observer.first;
    const base::Value& old_value =
        device_settings_cache_.find(policy_name)->second;
    base::Value new_value = GetDeviceSettingValueForCache(policy_name);
    if (old_value != new_value) {
      // Update cache entry and notify observer.
      device_settings_cache_[policy_name] = std::move(new_value);
      observer.second->Notify();
    }
  }
}

base::CallbackListSubscription
DeviceReportingSettingsLacros::AddSettingsObserver(
    const std::string& policy_name,
    base::RepeatingClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!policy_name.empty());
  DCHECK(callback);
  DCHECK(IsSupportedPolicy(policy_name))
      << "Unsupported device reporting setting in Lacros";

  // Get the callback registry associated with the policy.
  auto observer_result = observer_map_.emplace(
      policy_name, std::make_unique<base::RepeatingClosureList>());
  base::RepeatingClosureList* registry = observer_result.first->second.get();

  // Also add current setting value to the device settings cache so we can
  // derive updates.
  base::Value value = GetDeviceSettingValueForCache(policy_name);
  device_settings_cache_[policy_name] = std::move(value);

  return registry->Add(std::move(callback));
}

bool DeviceReportingSettingsLacros::PrepareTrustedValues(
    base::OnceClosure callback) {
  // Device settings retrieved via crosapi are only populated if the device is
  // trusted.
  return true;
}

bool DeviceReportingSettingsLacros::GetBoolean(const std::string& policy_name,
                                               bool* out_value) const {
  crosapi::mojom::DeviceSettings* const device_settings =
      delegate_->GetDeviceSettings();

  if (policy_name == ::policy::key::kReportDeviceNetworkStatus) {
    *out_value = (device_settings->report_device_network_status ==
                  ::crosapi::mojom::DeviceSettings::OptionalBool::kTrue);
    return true;
  }

  // Cannot retrieve device reporting setting for the given policy via cros api.
  return false;
}

bool DeviceReportingSettingsLacros::GetInteger(const std::string& policy_name,
                                               int* out_value) const {
  crosapi::mojom::DeviceSettings* const device_settings =
      delegate_->GetDeviceSettings();

  if (policy_name == ::policy::key::kReportUploadFrequency &&
      !device_settings->report_upload_frequency.is_null()) {
    *out_value = device_settings->report_upload_frequency->value;
    return true;
  }

  if (policy_name ==
          ::policy::key::kReportDeviceNetworkTelemetryCollectionRateMs &&
      !device_settings->report_device_network_telemetry_collection_rate_ms
           .is_null()) {
    *out_value =
        device_settings->report_device_network_telemetry_collection_rate_ms
            ->value;
    return true;
  }

  // Cannot retrieve device reporting setting for the given policy via cros api.
  return false;
}

bool DeviceReportingSettingsLacros::GetList(
    const std::string& policy_name,
    const base::Value::List** out_value) const {
  DCHECK(out_value);

  // No use cases for this yet.
  NOTIMPLEMENTED();
  return false;
}

const base::Value DeviceReportingSettingsLacros::GetDeviceSettingValueForCache(
    const std::string& policy_name) {
  bool bool_value;
  if (GetBoolean(policy_name, &bool_value)) {
    return base::Value(bool_value);
  }

  int int_value;
  if (GetInteger(policy_name, &int_value)) {
    return base::Value(int_value);
  }

  // We should never get here since we only observe supported device settings
  // but we return an empty value anyway.
  return base::Value();
}

}  // namespace reporting

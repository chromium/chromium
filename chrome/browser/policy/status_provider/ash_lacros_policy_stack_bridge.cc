// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/ash_lacros_policy_stack_bridge.h"

#include <utility>

#include "base/callback_forward.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/policy_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/policy/core/browser/policy_conversions.h"

AshLacrosPolicyStackBridge::AshLacrosPolicyStackBridge() {
  LoadDevicePolicy();
}

AshLacrosPolicyStackBridge::~AshLacrosPolicyStackBridge() {}

base::Value::Dict AshLacrosPolicyStackBridge::GetStatus() {
  return device_policy_status_.Clone();
}

void AshLacrosPolicyStackBridge::GetValues(
    base::Value::List& out_policy_values) {
  if (device_policy_.empty())
    return;
  for (auto& value : out_policy_values) {
    base::Value::Dict* dict = value.GetIfDict();
    if (!dict)
      continue;
    const std::string* id = dict->FindString(policy::kIdKey);
    if (id == nullptr || *id != policy::kChromePoliciesId)
      continue;
    base::Value::Dict* chrome_policies = dict->FindDict(policy::kPoliciesKey);
    if (chrome_policies)
      chrome_policies->Merge(device_policy_.Clone());
  }
}

base::Value::Dict AshLacrosPolicyStackBridge::GetNames() {
  return {};
}

void AshLacrosPolicyStackBridge::Refresh() {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::DeviceSettingsService>()) {
    return;
  }
  // Send request to Ash to reload the policy. This will reload the device
  // policy and the device account policy. Then Ash will send the updates to
  // Lacros the same way it happens when that policy gets invalidated.
  // TODO(crbug.com/1260935): Add here the request for remote commands to be
  // sent.
  service->GetRemote<crosapi::mojom::PolicyService>()->ReloadPolicy();
  NotifyValueChange();
}

void AshLacrosPolicyStackBridge::LoadDevicePolicy() {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::DeviceSettingsService>()) {
    return;
  }
  if (service->GetInterfaceVersion(
          crosapi::mojom::DeviceSettingsService::Uuid_) >=
      static_cast<int>(
          crosapi::mojom::DeviceSettingsService::kGetDevicePolicyMinVersion)) {
    service->GetRemote<crosapi::mojom::DeviceSettingsService>()
        ->GetDevicePolicy(
            base::BindOnce(&AshLacrosPolicyStackBridge::OnDevicePolicyLoaded,
                           weak_ptr_factory_.GetWeakPtr()));
  } else if (service->GetInterfaceVersion(
                 crosapi::mojom::DeviceSettingsService::Uuid_) >=
             static_cast<int>(crosapi::mojom::DeviceSettingsService::
                                  kGetDevicePolicyDeprecatedMinVersion)) {
    service->GetRemote<crosapi::mojom::DeviceSettingsService>()
        ->GetDevicePolicyDeprecated(base::BindOnce(
            &AshLacrosPolicyStackBridge::OnDevicePolicyLoadedDeprecated,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void AshLacrosPolicyStackBridge::OnDevicePolicyLoadedDeprecated(
    base::Value device_policy,
    base::Value legend_data) {
  base::Value::Dict device_policy_dict;
  base::Value::Dict legend_data_dict;
  if (device_policy.is_dict()) {
    device_policy_dict = std::move(device_policy.GetDict());
  }
  if (legend_data.is_dict()) {
    legend_data_dict = std::move(legend_data.GetDict());
  }
  OnDevicePolicyLoaded(std::move(device_policy_dict),
                       std::move(legend_data_dict));
}

void AshLacrosPolicyStackBridge::OnDevicePolicyLoaded(
    base::Value::Dict device_policy,
    base::Value::Dict legend_data) {
  if (device_policy != device_policy_) {
    device_policy_ = std::move(device_policy);
    device_policy_status_ = std::move(legend_data);
  }
  NotifyStatusChange();
  NotifyValueChange();
}

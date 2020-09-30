// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_update_policy_fetcher_win.h"

#include <wrl/client.h>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_bstr.h"
#include "chrome/install_static/install_util.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "google_update/google_update_idl.h"

namespace {

constexpr char kAutoUpdateCheckPeriodMinutes[] = "AutoUpdateCheckPeriodMinutes";
constexpr char kDownloadPreference[] = "DownloadPreference";
constexpr char kInstallPolicy[] = "InstallPolicy";
constexpr char kRollbackToTargetVersion[] = "RollbackToTargetVersion";
constexpr char kTargetVersionPrefix[] = "TargetVersionPrefix";
constexpr char kUpdatePolicy[] = "UpdatePolicy";
constexpr char kUpdatesSuppressedDurationMin[] = "UpdatesSuppressedDurationMin";
constexpr char kUpdatesSuppressedStartHour[] = "UpdatesSuppressedStartHour";
constexpr char kUpdatesSuppressedStartMinute[] = "UpdatesSuppressedStartMinute";

// Adds the |value| of |policy_name| to |policies| using a "Mandatory" level,
// "Machine" scope and "Platform" source.
void AddPolicy(const char* policy_name,
               base::Value value,
               policy::PolicyMap* policies) {
  DCHECK(policies);
  policies->Set(policy_name, policy::POLICY_LEVEL_MANDATORY,
                policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
                std::move(value), nullptr);
}

}  // namespace

base::Value GetGoogleUpdatePolicyNames() {
  base::Value names(base::Value::Type::LIST);
  for (const auto& key_value : GetGoogleUpdatePolicySchemas())
    names.Append(base::Value(key_value.first));
  return names;
}

policy::PolicyConversions::PolicyToSchemaMap GetGoogleUpdatePolicySchemas() {
  // TODO(ydago): Use actual schemas.
  return policy::PolicyConversions::PolicyToSchemaMap{{
      {kAutoUpdateCheckPeriodMinutes, policy::Schema()},
      {kDownloadPreference, policy::Schema()},
      {kInstallPolicy, policy::Schema()},
      {kRollbackToTargetVersion, policy::Schema()},
      {kTargetVersionPrefix, policy::Schema()},
      {kUpdatePolicy, policy::Schema()},
      {kUpdatesSuppressedDurationMin, policy::Schema()},
      {kUpdatesSuppressedStartHour, policy::Schema()},
      {kUpdatesSuppressedStartMinute, policy::Schema()},
  }};
}

std::unique_ptr<policy::PolicyMap> GetGoogleUpdatePolicies() {
  base::win::AssertComInitialized();
  Microsoft::WRL::ComPtr<IPolicyStatus> policy_status;
  HRESULT last_com_res =
      ::CoCreateInstance(CLSID_PolicyStatusMachineClass, nullptr, CLSCTX_ALL,
                         IID_PPV_ARGS(&policy_status));

  // If PolicyStatusMachineClass cannot be instantiated or does not expose
  // IPolicyStatus, assume the updater is not configured yet to return the
  // policies.
  if (FAILED(last_com_res))
    return nullptr;

  auto policies = std::make_unique<policy::PolicyMap>();
  base::win::ScopedBstr app_id(install_static::GetAppGuid());

  DWORD auto_update_check_period_minutes;
  last_com_res = policy_status->get_lastCheckPeriodMinutes(
      &auto_update_check_period_minutes);
  if (SUCCEEDED(last_com_res)) {
    AddPolicy(kAutoUpdateCheckPeriodMinutes,
              base::Value(
                  base::saturated_cast<int>(auto_update_check_period_minutes)),
              policies.get());
  }

  base::win::ScopedBstr download_preference_group_policy;
  last_com_res = policy_status->get_downloadPreferenceGroupPolicy(
      download_preference_group_policy.Receive());
  if (SUCCEEDED(last_com_res) &&
      download_preference_group_policy.Length() > 0) {
    AddPolicy(kDownloadPreference,
              base::Value(download_preference_group_policy.Get()),
              policies.get());
  }

  DWORD effective_policy_for_app_installs;
  last_com_res = policy_status->get_effectivePolicyForAppInstalls(
      app_id.Get(), &effective_policy_for_app_installs);
  if (SUCCEEDED(last_com_res)) {
    AddPolicy(kInstallPolicy,
              base::Value(
                  base::saturated_cast<int>(effective_policy_for_app_installs)),
              policies.get());
  }

  DWORD effective_policy_for_app_updates;
  last_com_res = policy_status->get_effectivePolicyForAppUpdates(
      app_id.Get(), &effective_policy_for_app_updates);
  if (SUCCEEDED(last_com_res)) {
    AddPolicy(kUpdatePolicy,
              base::Value(
                  base::saturated_cast<int>(effective_policy_for_app_updates)),
              policies.get());
  }

  DWORD updates_suppressed_duration;
  DWORD updates_suppressed_start_hour;
  DWORD updates_suppressed_start_minute;
  VARIANT_BOOL are_updates_suppressed;
  last_com_res = policy_status->get_updatesSuppressedTimes(
      &updates_suppressed_start_hour, &updates_suppressed_start_minute,
      &updates_suppressed_duration, &are_updates_suppressed);
  if (SUCCEEDED(last_com_res)) {
    AddPolicy(
        kUpdatesSuppressedDurationMin,
        base::Value(base::saturated_cast<int>(updates_suppressed_duration)),
        policies.get());
    AddPolicy(
        kUpdatesSuppressedStartHour,
        base::Value(base::saturated_cast<int>(updates_suppressed_start_hour)),
        policies.get());
    AddPolicy(
        kUpdatesSuppressedStartMinute,
        base::Value(base::saturated_cast<int>(updates_suppressed_start_minute)),
        policies.get());
  }

  VARIANT_BOOL is_rollback_to_target_version_allowed;
  last_com_res = policy_status->get_isRollbackToTargetVersionAllowed(
      app_id.Get(), &is_rollback_to_target_version_allowed);
  if (SUCCEEDED(last_com_res)) {
    AddPolicy(
        kRollbackToTargetVersion,
        base::Value(is_rollback_to_target_version_allowed == VARIANT_TRUE),
        policies.get());
  }

  base::win::ScopedBstr target_version_prefix;
  last_com_res = policy_status->get_targetVersionPrefix(
      app_id.Get(), target_version_prefix.Receive());
  if (SUCCEEDED(last_com_res) && target_version_prefix.Length() > 0) {
    AddPolicy(kTargetVersionPrefix, base::Value(target_version_prefix.Get()),
              policies.get());
  }

  return policies;
}

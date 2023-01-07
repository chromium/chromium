// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_update_policy_fetcher_win.h"

#include <ATLComTime.h>
#include <wrl/client.h>

#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_bstr.h"
#include "chrome/browser/component_updater/updater_state.h"
#include "chrome/browser/google/google_update_policy_fetcher_win_util.h"
#include "chrome/install_static/install_util.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/strings/grit/components_strings.h"
#include "google_update/google_update_idl.h"

namespace {

constexpr char kAutoUpdateCheckPeriodMinutes[] = "AutoUpdateCheckPeriodMinutes";
constexpr char kDownloadPreference[] = "DownloadPreference";
constexpr char kInstallPolicy[] = "InstallPolicy";
constexpr char kProxyMode[] = "ProxyMode";
constexpr char kProxyPacUrl[] = "ProxyPacUrl";
constexpr char kProxyServer[] = "ProxyServer";
constexpr char kRollbackToTargetVersion[] = "RollbackToTargetVersion";
constexpr char kTargetVersionPrefix[] = "TargetVersionPrefix";
constexpr char kTargetChannel[] = "TargetChannel";
constexpr char kUpdatePolicy[] = "UpdatePolicy";
constexpr char kUpdatesSuppressedDurationMin[] = "UpdatesSuppressedDurationMin";
constexpr char kUpdatesSuppressedStartHour[] = "UpdatesSuppressedStartHour";
constexpr char kUpdatesSuppressedStartMinute[] = "UpdatesSuppressedStartMinute";

// Adds the |value| of |policy_name| to |policies| using a "Mandatory" level,
// "Machine" scope and "Platform" source.
void AddPolicy(const char* policy_name,
               base::Value value,
               policy::PolicyMap& policies) {
  policies.Set(policy_name, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
               std::move(value), nullptr);
}

// Adds the policy |policy_name| extracted from |policy| into |policies|.
// |value_override_function| is an optional function that modifies and overrides
// the value of the policy that needs to be store in |policies|. This function
// may be used to convert the extracted value, that is always a string into
// other types or formats.
void AddPolicy(const char* policy_name,
               IPolicyStatusValue* policy,
               policy::PolicyMap& policies,
               const PolicyValueOverrideFunction& value_override_function =
                   PolicyValueOverrideFunction()) {
  auto policy_entry =
      ConvertPolicyStatusValueToPolicyEntry(policy, value_override_function);
  if (policy_entry)
    policies.Set(policy_name, std::move(*policy_entry));
}

base::Time DateToTime(DATE date) {
  ::COleDateTime date_time(date);
  base::Time time;
  if (date_time.m_status == ::COleDateTime::valid) {
    std::ignore = base::Time::FromLocalExploded(
        {date_time.GetYear(), date_time.GetMonth(), date_time.GetDayOfWeek(),
         date_time.GetDay(), date_time.GetHour(), date_time.GetMinute(),
         date_time.GetSecond(), 0},
        &time);
  }
  return time;
}

// Returns the Google Update policies as of release 1.3.36.21.
std::unique_ptr<policy::PolicyMap> GetGoogleUpdatePolicies(
    IPolicyStatus2* policy_status) {
  DCHECK(policy_status);

  policy_status->refreshPolicies();
  auto policies = std::make_unique<policy::PolicyMap>();
  base::win::ScopedBstr app_id(install_static::GetAppGuid());

  {
    Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
    if (SUCCEEDED(policy_status->get_lastCheckPeriodMinutes(&policy)))
      AddPolicy(kAutoUpdateCheckPeriodMinutes, policy.Get(), *policies);
  }
  {
    Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
    if (SUCCEEDED(policy_status->get_downloadPreferenceGroupPolicy(&policy)))
      AddPolicy(kDownloadPreference, policy.Get(), *policies);
  }
  {
    Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
    if (SUCCEEDED(policy_status->get_effectivePolicyForAppInstalls(app_id.Get(),
                                                                   &policy))) {
      AddPolicy(kInstallPolicy, policy.Get(), *policies);
    }
  }
  {
    Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
    if (SUCCEEDED(policy_status->get_effectivePolicyForAppUpdates(app_id.Get(),
                                                                  &policy))) {
      AddPolicy(kUpdatePolicy, policy.Get(), *policies);
    }
  }
  {
    Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
    VARIANT_BOOL are_updates_suppressed = VARIANT_FALSE;
    if (SUCCEEDED(policy_status->get_updatesSuppressedTimes(
            &policy, &are_updates_suppressed))) {
      // A function that extracts the |index|-th value from a comma-separated
      // |initial_value|.
      const auto extract_value = [](int index, BSTR initial_value) {
        auto split = base::SplitString(
            initial_value, L",", base::WhitespaceHandling::TRIM_WHITESPACE,
            base::SplitResult::SPLIT_WANT_NONEMPTY);
        return base::Value(
            base::WideToUTF8(split.size() == 3 ? split[index] : L""));
      };
      AddPolicy(kUpdatesSuppressedStartHour, policy.Get(), *policies,
                base::BindRepeating(extract_value, 0));
      AddPolicy(kUpdatesSuppressedStartMinute, policy.Get(), *policies,
                base::BindRepeating(extract_value, 1));
      AddPolicy(kUpdatesSuppressedDurationMin, policy.Get(), *policies,
                base::BindRepeating(extract_value, 2));
    }
  }
  {
    Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
    if (SUCCEEDED(policy_status->get_isRollbackToTargetVersionAllowed(
            app_id.Get(), &policy))) {
      AddPolicy(kRollbackToTargetVersion, policy.Get(), *policies);
    }
  }
  {
    Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
    if (SUCCEEDED(
            policy_status->get_targetVersionPrefix(app_id.Get(), &policy))) {
      AddPolicy(kTargetVersionPrefix, policy.Get(), *policies);
    }
  }
  {
    Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
    if (SUCCEEDED(policy_status->get_targetChannel(app_id.Get(), &policy)))
      AddPolicy(kTargetChannel, policy.Get(), *policies);
  }
  {
    Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
    if (SUCCEEDED(policy_status->get_proxyMode(&policy)))
      AddPolicy(kProxyMode, policy.Get(), *policies);
  }
  {
    Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
    if (SUCCEEDED(policy_status->get_proxyPacUrl(&policy)))
      AddPolicy(kProxyPacUrl, policy.Get(), *policies);
  }
  {
    Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
    if (SUCCEEDED(policy_status->get_proxyServer(&policy)))
      AddPolicy(kProxyServer, policy.Get(), *policies);
  }

  return policies;
}

// Returns the Google Update policies as of release 1.3.35.331
std::unique_ptr<policy::PolicyMap> GetLegacyGoogleUpdatePolicies(
    IPolicyStatus* policy_status) {
  DCHECK(policy_status);
  auto policies = std::make_unique<policy::PolicyMap>();
  base::win::ScopedBstr app_id(install_static::GetAppGuid());

  DWORD auto_update_check_period_minutes;
  HRESULT last_com_res = policy_status->get_lastCheckPeriodMinutes(
      &auto_update_check_period_minutes);
  if (SUCCEEDED(last_com_res)) {
    AddPolicy(kAutoUpdateCheckPeriodMinutes,
              base::Value(
                  base::saturated_cast<int>(auto_update_check_period_minutes)),
              *policies);
  }

  base::win::ScopedBstr download_preference_group_policy;
  last_com_res = policy_status->get_downloadPreferenceGroupPolicy(
      download_preference_group_policy.Receive());
  if (SUCCEEDED(last_com_res) &&
      download_preference_group_policy.Length() > 0) {
    AddPolicy(kDownloadPreference,
              base::Value(base::AsStringPiece16(
                  download_preference_group_policy.Get())),
              *policies);
  }

  DWORD effective_policy_for_app_installs;
  last_com_res = policy_status->get_effectivePolicyForAppInstalls(
      app_id.Get(), &effective_policy_for_app_installs);
  if (SUCCEEDED(last_com_res)) {
    AddPolicy(kInstallPolicy,
              base::Value(
                  base::saturated_cast<int>(effective_policy_for_app_installs)),
              *policies);
  }

  DWORD effective_policy_for_app_updates;
  last_com_res = policy_status->get_effectivePolicyForAppUpdates(
      app_id.Get(), &effective_policy_for_app_updates);
  if (SUCCEEDED(last_com_res)) {
    AddPolicy(kUpdatePolicy,
              base::Value(
                  base::saturated_cast<int>(effective_policy_for_app_updates)),
              *policies);
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
        *policies);
    AddPolicy(
        kUpdatesSuppressedStartHour,
        base::Value(base::saturated_cast<int>(updates_suppressed_start_hour)),
        *policies);
    AddPolicy(
        kUpdatesSuppressedStartMinute,
        base::Value(base::saturated_cast<int>(updates_suppressed_start_minute)),
        *policies);
  }

  VARIANT_BOOL is_rollback_to_target_version_allowed;
  last_com_res = policy_status->get_isRollbackToTargetVersionAllowed(
      app_id.Get(), &is_rollback_to_target_version_allowed);
  if (SUCCEEDED(last_com_res)) {
    AddPolicy(
        kRollbackToTargetVersion,
        base::Value(is_rollback_to_target_version_allowed == VARIANT_TRUE),
        *policies);
  }

  base::win::ScopedBstr target_version_prefix;
  last_com_res = policy_status->get_targetVersionPrefix(
      app_id.Get(), target_version_prefix.Receive());
  if (SUCCEEDED(last_com_res) && target_version_prefix.Length() > 0) {
    AddPolicy(kTargetVersionPrefix,
              base::Value(base::AsStringPiece16(target_version_prefix.Get())),
              *policies);
  }

  return policies;
}

// Returns the state for versions prior to release 1.3.36.21.
std::unique_ptr<GoogleUpdateState> GetLegacyGoogleUpdateState() {
  component_updater::UpdaterState::Attributes state =
      component_updater::UpdaterState::GetState(
          install_static::IsSystemInstall());
  auto result = std::make_unique<GoogleUpdateState>();
  const auto version = state.find("version");
  if (version != state.end())
    result->version = base::ASCIIToWide(version->second);
  return result;
}

// Returns the state for release 1.3.36.21 and newer.
std::unique_ptr<GoogleUpdateState> GetGoogleUpdateState(
    IPolicyStatus2* policy_status) {
  DCHECK(policy_status);
  auto state = std::make_unique<GoogleUpdateState>();
  base::win::ScopedBstr updater_version;
  HRESULT last_com_res =
      policy_status->get_updaterVersion(updater_version.Receive());
  if (SUCCEEDED(last_com_res)) {
    DCHECK(updater_version.Length());
    state->version.assign(updater_version.Get(), updater_version.Length());
  }

  DATE last_checked_time;
  last_com_res = policy_status->get_lastCheckedTime(&last_checked_time);
  if (SUCCEEDED(last_com_res))
    state->last_checked_time = DateToTime(last_checked_time);

  return state;
}

}  // namespace

GoogleUpdatePoliciesAndState::GoogleUpdatePoliciesAndState() = default;

GoogleUpdatePoliciesAndState::~GoogleUpdatePoliciesAndState() = default;

base::Value GetGoogleUpdatePolicyNames() {
  base::Value names(base::Value::Type::LIST);
  for (const auto& key_value : GetGoogleUpdatePolicySchemas())
    names.Append(base::Value(key_value.first));
  return names;
}

policy::PolicyConversions::PolicyToSchemaMap GetGoogleUpdatePolicySchemas() {
  // TODO(crbug/1133309): Use actual schemas.
  return policy::PolicyConversions::PolicyToSchemaMap{{
      {kAutoUpdateCheckPeriodMinutes, policy::Schema()},
      {kDownloadPreference, policy::Schema()},
      {kInstallPolicy, policy::Schema()},
      {kProxyMode, policy::Schema()},
      {kProxyPacUrl, policy::Schema()},
      {kProxyServer, policy::Schema()},
      {kRollbackToTargetVersion, policy::Schema()},
      {kTargetVersionPrefix, policy::Schema()},
      {kTargetChannel, policy::Schema()},
      {kUpdatePolicy, policy::Schema()},
      {kUpdatesSuppressedDurationMin, policy::Schema()},
      {kUpdatesSuppressedStartHour, policy::Schema()},
      {kUpdatesSuppressedStartMinute, policy::Schema()},
  }};
}

std::unique_ptr<GoogleUpdatePoliciesAndState>
GetGoogleUpdatePoliciesAndState() {
  base::win::AssertComInitialized();
  Microsoft::WRL::ComPtr<IPolicyStatus2> policy_status2;
  Microsoft::WRL::ComPtr<IPolicyStatus> policy_status;
  auto policies_and_state = std::make_unique<GoogleUpdatePoliciesAndState>();
  bool is_system_install = install_static::IsSystemInstall();
  // The PolicyStatus{Machine,User}Class was introduced in Google
  // Update 1.3.36.21. If the IPolicyStatus2 interface cannot be found on the
  // relevant class, try to use the IPolicyStatus interface on
  // PolicyStatusMachineClass (introduced in 1.3.35.331).
  if (SUCCEEDED(::CoCreateInstance(
          is_system_install ? CLSID_PolicyStatusMachineClass
                            : CLSID_PolicyStatusUserClass,
          nullptr, CLSCTX_ALL, IID_PPV_ARGS(&policy_status2)))) {
    policies_and_state->policies =
        GetGoogleUpdatePolicies(policy_status2.Get());
    policies_and_state->state = GetGoogleUpdateState(policy_status2.Get());
  } else if (SUCCEEDED(::CoCreateInstance(CLSID_PolicyStatusMachineClass,
                                          nullptr, CLSCTX_ALL,
                                          IID_PPV_ARGS(&policy_status)))) {
    policies_and_state->policies =
        GetLegacyGoogleUpdatePolicies(policy_status.Get());
    policies_and_state->state = GetLegacyGoogleUpdateState();
  }

  return policies_and_state;
}

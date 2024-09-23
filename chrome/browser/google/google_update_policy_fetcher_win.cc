// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_update_policy_fetcher_win.h"

#include <ATLComTime.h>
#include <wrl/client.h>

#include <tuple>
#include <utility>

#include "base/check_op.h"
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
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/strings/grit/components_strings.h"

namespace {

// TODO(crbug.com/40271852): Add unit tests for these GoogleUpdate policies.
constexpr char kAutoUpdateCheckPeriodMinutes[] = "AutoUpdateCheckPeriodMinutes";
constexpr char kDownloadPreference[] = "DownloadPreference";
constexpr char kForceInstallApps[] = "ForceInstallApps";
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
constexpr char kCloudPolicyOverridesPlatformPolicy[] =
    "CloudPolicyOverridesPlatformPolicy";

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

// Returns the policies from GoogleUpdate. Requires GoogleUpdate
// version 1.3.36.91 (released 07-02-2021) or newer.
std::unique_ptr<policy::PolicyMap> GetGoogleUpdatePolicies(
    IPolicyStatus3* policy_status) {
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
    Microsoft::WRL::ComPtr<IPolicyStatus4> policy_status4;
    Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
    if (SUCCEEDED(policy_status->QueryInterface(
            install_static::IsSystemInstall() ? __uuidof(IPolicyStatus4System)
                                              : __uuidof(IPolicyStatus4User),
            IID_PPV_ARGS_Helper(&policy_status4))) &&
        SUCCEEDED(
            policy_status4->get_cloudPolicyOverridesPlatformPolicy(&policy))) {
      AddPolicy(kCloudPolicyOverridesPlatformPolicy, policy.Get(), *policies);
    }
  }
  {
    Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
    if (SUCCEEDED(policy_status->get_forceInstallApps(
            install_static::IsSystemInstall(), &policy))) {
      AddPolicy(kForceInstallApps, policy.Get(), *policies);
    }
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
    if (SUCCEEDED(policy_status->get_targetChannel(app_id.Get(), &policy))) {
      AddPolicy(kTargetChannel, policy.Get(), *policies);
    }
  }
  {
    Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
    if (SUCCEEDED(policy_status->get_proxyMode(&policy))) {
      AddPolicy(kProxyMode, policy.Get(), *policies);
    }
  }
  {
    Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
    if (SUCCEEDED(policy_status->get_proxyPacUrl(&policy))) {
      AddPolicy(kProxyPacUrl, policy.Get(), *policies);
    }
  }
  {
    Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
    if (SUCCEEDED(policy_status->get_proxyServer(&policy))) {
      AddPolicy(kProxyServer, policy.Get(), *policies);
    }
  }

  return policies;
}

// Returns the state from GoogleUpdate. Requires GoogleUpdate version 1.3.36.91
// (released 07-02-2021) or newer.
std::unique_ptr<GoogleUpdateState> GetGoogleUpdateState(
    IPolicyStatus3* policy_status) {
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
  base::Value::List names;
  for (const auto& key_value : GetGoogleUpdatePolicySchemas())
    names.Append(base::Value(key_value.first));
  return base::Value(std::move(names));
}

policy::PolicyConversions::PolicyToSchemaMap GetGoogleUpdatePolicySchemas() {
  // TODO(crbug.com/40722467): Use actual schemas.
  return policy::PolicyConversions::PolicyToSchemaMap{{
      {kAutoUpdateCheckPeriodMinutes, policy::Schema()},
      {kDownloadPreference, policy::Schema()},
      {kForceInstallApps, policy::Schema()},
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
      {kCloudPolicyOverridesPlatformPolicy, policy::Schema()},
  }};
}

std::unique_ptr<GoogleUpdatePoliciesAndState>
GetGoogleUpdatePoliciesAndState() {
  base::win::AssertComInitialized();
  Microsoft::WRL::ComPtr<IPolicyStatus3> policy_status3;
  auto policies_and_state = std::make_unique<GoogleUpdatePoliciesAndState>();
  const bool is_system_install = install_static::IsSystemInstall();
  Microsoft::WRL::ComPtr<IUnknown> unknown;
  if (FAILED(::CoCreateInstance(is_system_install
                                    ? CLSID_PolicyStatusSystemClass
                                    : CLSID_PolicyStatusUserClass,
                                nullptr, CLSCTX_ALL, IID_PPV_ARGS(&unknown)))) {
    return policies_and_state;
  }

  // Chrome queries for the SxS IIDs first, with a fallback to the legacy IID.
  // Without this change, marshaling can load the typelib from the wrong hive
  // (HKCU instead of HKLM, or vice-versa).
  HRESULT hr = unknown.CopyTo(is_system_install ? __uuidof(IPolicyStatus3System)
                                                : __uuidof(IPolicyStatus3User),
                              IID_PPV_ARGS_Helper(&policy_status3));
  if (FAILED(hr)) {
    hr = unknown.As(&policy_status3);
    if (FAILED(hr)) {
      return policies_and_state;
    }
  }

  policies_and_state->policies = GetGoogleUpdatePolicies(policy_status3.Get());
  policies_and_state->state = GetGoogleUpdateState(policy_status3.Get());

  return policies_and_state;
}

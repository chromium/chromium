// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/test/device_trust_management_mixin.h"

#include <utility>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/device_trust/test/test_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "components/device_signals/core/browser/pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace enterprise_connectors::test {

namespace {

base::Value GetAllowedHostValue(const std::string& url) {
  base::Value::List list;
  list.Append(url);
  return base::Value(std::move(list));
}

base::Value GetEmptyListValue() {
  return base::Value(base::Value::List());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::vector<std::string> ParseUrlsValue(const base::Value& urls_value) {
  std::vector<std::string> url_strings;
  for (const base::Value& value : urls_value.GetList()) {
    url_strings.push_back(value.GetString());
  }
  return url_strings;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

ManagementContext ToManagementContext(
    const DeviceTrustConnectorState& device_trust_state) {
  ManagementContext management_context;
  management_context.affiliated = device_trust_state.affiliated;
  management_context.is_cloud_user_managed =
      device_trust_state.cloud_user_management_level.is_managed;
  management_context.is_cloud_machine_managed =
      device_trust_state.cloud_machine_management_level.is_managed;
  return management_context;
}

}  // namespace

DeviceTrustManagementMixin::DeviceTrustManagementMixin(
    InProcessBrowserTestMixinHost* host,
    InProcessBrowserTest* test_base,
    DeviceTrustConnectorState device_trust_state)
    : InProcessBrowserTestMixin(host),
      test_base_(test_base),
      device_trust_state_(std::move(device_trust_state)),
      management_context_mixin_(ManagementContextMixin::Create(
          host,
          test_base,
          ToManagementContext(device_trust_state_))) {}

DeviceTrustManagementMixin::~DeviceTrustManagementMixin() = default;

void DeviceTrustManagementMixin::ManageCloudUser() {
  CHECK(!device_trust_state_.cloud_user_management_level.is_managed);
  device_trust_state_.cloud_user_management_level.is_managed = true;
  management_context_mixin_->ManageCloudUser();
}

void DeviceTrustManagementMixin::EnableMachineInlinePolicy(
    const std::string& url) {
  SetMachineInlinePolicy(GetAllowedHostValue(url));
}

void DeviceTrustManagementMixin::DisableMachineInlinePolicy() {
  SetMachineInlinePolicy(GetEmptyListValue());
}

void DeviceTrustManagementMixin::EnableUserInlinePolicy(
    const std::string& url) {
  SetUserInlinePolicy(GetAllowedHostValue(url));
}

void DeviceTrustManagementMixin::DisableUserInlinePolicy() {
  SetUserInlinePolicy(GetEmptyListValue());
}

void DeviceTrustManagementMixin::DisableAllInlinePolicies() {
  if (device_trust_state_.cloud_user_management_level.is_managed) {
    DisableUserInlinePolicy();
  }

  if (device_trust_state_.cloud_machine_management_level.is_managed) {
    DisableMachineInlinePolicy();
  }
}

void DeviceTrustManagementMixin::SetConsentGiven(bool consent_given) {
  device_trust_state_.consent_given = consent_given;
  test_base_->browser()->profile()->GetPrefs()->SetBoolean(
      device_signals::prefs::kDeviceSignalsConsentReceived, consent_given);
}

void DeviceTrustManagementMixin::SetPermanentConsentGiven(
    bool permanent_consent_given) {
  device_trust_state_.permanent_consent_given = permanent_consent_given;
  test_base_->browser()->profile()->GetPrefs()->SetBoolean(
      device_signals::prefs::kDeviceSignalsPermanentConsentReceived,
      permanent_consent_given);
}

void DeviceTrustManagementMixin::SetUpOnMainThread() {
  if (device_trust_state_.cloud_machine_management_level
          .is_inline_policy_enabled) {
    EnableMachineInlinePolicy();
  }

  if (device_trust_state_.cloud_user_management_level
          .is_inline_policy_enabled) {
    EnableUserInlinePolicy();
  }

  SetConsentGiven(device_trust_state_.consent_given);
  SetPermanentConsentGiven(device_trust_state_.permanent_consent_given);
}

void DeviceTrustManagementMixin::SetMachineInlinePolicy(
    base::Value policy_value) {
  CHECK(device_trust_state_.cloud_machine_management_level.is_managed);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto device_policy_update =
      management_context_mixin_->RequestDevicePolicyUpdate();
  auto* allowed_urls_proto =
      device_policy_update->policy_payload()
          ->mutable_device_login_screen_context_aware_access_signals_allowlist();
  allowed_urls_proto->mutable_policy_options()->set_mode(
      enterprise_management::PolicyOptions::MANDATORY);
  auto* policy_string_list = allowed_urls_proto->mutable_value();
  const auto urls = ParseUrlsValue(policy_value);

  if (urls.empty()) {
    policy_string_list->Clear();
  } else {
    for (const auto& url : urls) {
      policy_string_list->add_entries(url.c_str());
    }
  }
#else
  base::flat_map<std::string, std::optional<base::Value>> policy_values;
  policy_values.insert({policy::key::kBrowserContextAwareAccessSignalsAllowlist,
                        std::move(policy_value)});
  management_context_mixin_->SetCloudMachinePolicies(std::move(policy_values));
#endif
}

void DeviceTrustManagementMixin::SetUserInlinePolicy(base::Value policy_value) {
  CHECK(device_trust_state_.cloud_user_management_level.is_managed);

  base::flat_map<std::string, std::optional<base::Value>> policy_values;
  policy_values.insert({policy::key::kUserContextAwareAccessSignalsAllowlist,
                        std::move(policy_value)});
  management_context_mixin_->SetCloudUserPolicies(std::move(policy_values));
}

}  // namespace enterprise_connectors::test

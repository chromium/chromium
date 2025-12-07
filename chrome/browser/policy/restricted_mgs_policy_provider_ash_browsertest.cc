// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "base/values.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/certificates_handler.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

const char kEmail[] = "user@test";

const PolicyNamespace kChromeNamespace(POLICY_DOMAIN_CHROME, std::string());

PolicyMap GetExpectedRestrictedPolicies() {
  PolicyMap policy_map;
  policy_map.Set(key::kArcEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                 base::Value(false), nullptr);
  policy_map.Set(key::kCrostiniAllowed, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                 base::Value(false), nullptr);
  policy_map.Set(key::kDeletePrintJobHistoryAllowed, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                 base::Value(true), nullptr);
  policy_map.Set(key::kKerberosEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                 base::Value(false), nullptr);
  policy_map.Set(key::kNetworkFileSharesAllowed, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                 base::Value(false), nullptr);
  policy_map.Set(key::kUserBorealisAllowed, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                 base::Value(false), nullptr);
  policy_map.Set(key::kUserPluginVmAllowed, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                 base::Value(false), nullptr);
  policy_map.Set(key::kAllowDeletingBrowserHistory, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                 base::Value(true), nullptr);
  policy_map.Set(
      key::kCACertificateManagementAllowed, POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_USER,
      POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
      base::Value(static_cast<int>(CACertificateManagementPermission::kNone)),
      nullptr);
  policy_map.Set(key::kClientCertificateManagementAllowed,
                 POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                 base::Value(static_cast<int>(
                     ClientCertificateManagementPermission::kNone)),
                 nullptr);
  policy_map.Set(key::kEnableMediaRouter, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                 base::Value(false), nullptr);
  policy_map.Set(key::kPasswordManagerEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                 base::Value(false), nullptr);
  policy_map.Set(key::kScreenCaptureAllowed, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                 base::Value(false), nullptr);
  return policy_map;
}

}  // namespace

class RestrictedMGSPolicyProviderAshBrowserTest
    : public DevicePolicyCrosBrowserTest {
 public:
  // DevicePolicyCrosBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kLoginManager);
    command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
    command_line->AppendSwitch(ash::switches::kOobeSkipPostLogin);
  }

  void SetUpPolicy(bool restricted) {
    em::DeviceLocalAccountsProto* const device_local_accounts =
        device_policy()->payload().mutable_device_local_accounts();
    em::DeviceLocalAccountInfoProto* const account =
        device_local_accounts->add_account();
    account->set_account_id(kEmail);
    account->set_type(
        em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);
    device_local_accounts->set_auto_login_id(kEmail);
    device_local_accounts->set_auto_login_delay(0);
    SetRestrictedPolicy(restricted);
    RefreshDevicePolicy();
  }

  void SetRestrictedPolicy(bool restricted) {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.mutable_device_restricted_managed_guest_session_enabled()
        ->set_enabled(restricted);
    policy_helper()->RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
        {ash::kDeviceRestrictedManagedGuestSessionEnabled});
  }

  PolicyMap GetCurrentChromePolicies() {
    auto* profile = ProfileManager::GetPrimaryUserProfile();
    auto* policy_connector = profile->GetProfilePolicyConnector();
    return policy_connector->policy_service()
        ->GetPolicies(kChromeNamespace)
        .Clone();
  }

 protected:
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(RestrictedMGSPolicyProviderAshBrowserTest,
                       DeviceRestrictedManagedGuestSessionDisabled) {
  SetUpPolicy(/*restricted=*/false);
  ash::SessionStateWaiter(session_manager::SessionState::ACTIVE).Wait();

  const auto current_policy_map = GetCurrentChromePolicies();

  for (const auto& [policy_name, entry] : current_policy_map) {
    EXPECT_NE(entry.source,
              POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE)
        << "Policy " << policy_name
        << " should not have the restricted MGS override source when the "
           "setting is disabled.";
  }
}

IN_PROC_BROWSER_TEST_F(RestrictedMGSPolicyProviderAshBrowserTest,
                       DeviceRestrictedManagedGuestSessionEnabled) {
  SetUpPolicy(/*restricted=*/true);
  ash::SessionStateWaiter(session_manager::SessionState::ACTIVE).Wait();

  const auto current_policy_map = GetCurrentChromePolicies();
  const auto expected_restricted_policies = GetExpectedRestrictedPolicies();

  // Check that all expected restricted policies are set correctly.
  for (const auto& [policy_name, expected_entry] :
       expected_restricted_policies) {
    const PolicyMap::Entry* actual_entry = current_policy_map.Get(policy_name);
    ASSERT_TRUE(actual_entry) << "Policy " << policy_name << " is missing";
    EXPECT_TRUE(expected_entry.Equals(*actual_entry))
        << "Policy " << policy_name << " has wrong value or attributes";
  }

  // Check that no other policies have the restricted source.
  for (const auto& [policy_name, actual_entry] : current_policy_map) {
    if (expected_restricted_policies.Get(policy_name)) {
      continue;  // Already checked above.
    }
    EXPECT_NE(actual_entry.source,
              POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE)
        << "Policy " << policy_name
        << " has the restricted MGS override source but is not in the "
           "expected set.";
  }
}

}  // namespace policy

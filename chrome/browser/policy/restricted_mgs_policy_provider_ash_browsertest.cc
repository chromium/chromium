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

void AddRestrictedPoliciesToMap(PolicyMap* policy_map) {
  policy_map->Set(key::kArcEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                  base::Value(false), nullptr);
  policy_map->Set(key::kCrostiniAllowed, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER,
                  POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                  base::Value(false), nullptr);
  policy_map->Set(key::kDeletePrintJobHistoryAllowed, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER,
                  POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                  base::Value(true), nullptr);
  policy_map->Set(key::kKerberosEnabled, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER,
                  POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                  base::Value(false), nullptr);
  policy_map->Set(key::kNetworkFileSharesAllowed, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER,
                  POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                  base::Value(false), nullptr);
  policy_map->Set(key::kUserBorealisAllowed, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER,
                  POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                  base::Value(false), nullptr);
  policy_map->Set(key::kUserPluginVmAllowed, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER,
                  POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                  base::Value(false), nullptr);
  policy_map->Set(key::kAllowDeletingBrowserHistory, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER,
                  POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                  base::Value(true), nullptr);
  policy_map->Set(
      key::kCACertificateManagementAllowed, POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_USER,
      POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
      base::Value(static_cast<int>(CACertificateManagementPermission::kNone)),
      nullptr);
  policy_map->Set(key::kClientCertificateManagementAllowed,
                  POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                  base::Value(static_cast<int>(
                      ClientCertificateManagementPermission::kNone)),
                  nullptr);
  policy_map->Set(key::kEnableMediaRouter, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER,
                  POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                  base::Value(false), nullptr);
  policy_map->Set(key::kPasswordManagerEnabled, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER,
                  POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                  base::Value(false), nullptr);
  policy_map->Set(key::kScreenCaptureAllowed, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER,
                  POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                  base::Value(false), nullptr);
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
    // Save base policy map before the RestrictedMGSPolicyProvider is created.
    SaveExpectedPolicyMap();
    RefreshDevicePolicy();
  }

  void SetRestrictedPolicy(bool restricted) {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.mutable_device_restricted_managed_guest_session_enabled()
        ->set_enabled(restricted);
    policy_helper()->RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
        {ash::kDeviceRestrictedManagedGuestSessionEnabled});
  }

  void SaveExpectedPolicyMap() {
    expected_policy_map_ = GetCurrentChromePolicies();
    SetEnterpriseUsersDefaults(&expected_policy_map_);

    // Values implicitly enforced for public accounts.
    expected_policy_map_.Set(key::kShelfAutoHideBehavior,
                             POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                             POLICY_SOURCE_ENTERPRISE_DEFAULT,
                             base::Value("Never"), nullptr);
    expected_policy_map_.Set(key::kShowLogoutButtonInTray,
                             POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                             POLICY_SOURCE_ENTERPRISE_DEFAULT,
                             base::Value(true), nullptr);
  }

  PolicyMap GetCurrentChromePolicies() {
    auto* profile = ProfileManager::GetPrimaryUserProfile();
    auto* policy_connector = profile->GetProfilePolicyConnector();
    return policy_connector->policy_service()
        ->GetPolicies(kChromeNamespace)
        .Clone();
  }

 protected:
  PolicyMap expected_policy_map_;
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(RestrictedMGSPolicyProviderAshBrowserTest,
                       DeviceRestrictedManagedGuestSessionDisabled) {
  SetUpPolicy(/*restricted=*/false);
  ash::SessionStateWaiter(session_manager::SessionState::ACTIVE).Wait();

  auto current_policy_map = GetCurrentChromePolicies();

  // Policy map stays unchanged.
  EXPECT_TRUE(expected_policy_map_.Equals(current_policy_map));
}

IN_PROC_BROWSER_TEST_F(RestrictedMGSPolicyProviderAshBrowserTest,
                       DeviceRestrictedManagedGuestSessionEnabled) {
  SetUpPolicy(/*restricted=*/true);
  ash::SessionStateWaiter(session_manager::SessionState::ACTIVE).Wait();

  auto current_policy_map = GetCurrentChromePolicies();

  // Policy map has the restricted policies.
  AddRestrictedPoliciesToMap(&expected_policy_map_);
  EXPECT_TRUE(expected_policy_map_.Equals(current_policy_map));
}

}  // namespace policy

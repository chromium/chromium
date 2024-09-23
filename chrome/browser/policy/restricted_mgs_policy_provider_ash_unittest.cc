// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ash/settings/cros_settings_holder.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/policy/restricted_mgs_policy_provider.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/webui/certificates_handler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/components/mgs/managed_guest_session_test_utils.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

std::unique_ptr<PolicyBundle> BuildRestrictedPolicyBundle() {
  auto policy_bundle = std::make_unique<PolicyBundle>();
  PolicyMap& policy_map =
      policy_bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
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

  return policy_bundle;
}

}  // namespace

class RestrictedMGSPolicyProviderAshTest : public ash::DeviceSettingsTestBase {
 public:
  void SetUp() override {
    ash::DeviceSettingsTestBase::SetUp();
    ash::LoginState::Initialize();
    cros_settings_holder_ = std::make_unique<ash::CrosSettingsHolder>(
        device_settings_service_.get(),
        TestingBrowserProcess::GetGlobal()->local_state());

    cros_settings_helper_ = std::make_unique<ash::ScopedCrosSettingsTestHelper>(
        /*create_service=*/false);
    cros_settings_helper_->ReplaceDeviceSettingsProviderWithStub();
  }

  void TearDown() override {
    cros_settings_helper_.reset();
    cros_settings_holder_.reset();
    ash::DeviceSettingsTestBase::TearDown();
    ash::LoginState::Shutdown();
  }

  std::unique_ptr<ash::CrosSettingsHolder> cros_settings_holder_;
  std::unique_ptr<ash::ScopedCrosSettingsTestHelper> cros_settings_helper_;
};

TEST_F(RestrictedMGSPolicyProviderAshTest, CreateRestrictedMGSPolicyProvider) {
  // Doesn't get created for a regular user.
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LOGGED_IN_ACTIVE,
      ash::LoginState::LOGGED_IN_USER_REGULAR);
  auto policy_provider = RestrictedMGSPolicyProvider::Create();
  EXPECT_FALSE(policy_provider);

  chromeos::FakeManagedGuestSession managed_guest_session(
      /*initialize_login_state=*/false);
  policy_provider = RestrictedMGSPolicyProvider::Create();
  EXPECT_TRUE(policy_provider);
}

TEST_F(RestrictedMGSPolicyProviderAshTest,
       DeviceRestrictedManagedGuestSessionDisabled) {
  cros_settings_helper_->SetBoolean(
      ash::kDeviceRestrictedManagedGuestSessionEnabled, false);
  // Empty policy bundle.
  PolicyMap expected_policy_map;
  PolicyBundle expected_policy_bundle;
  expected_policy_bundle.Get(PolicyNamespace(
      POLICY_DOMAIN_CHROME, std::string())) = expected_policy_map.Clone();

  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LOGGED_IN_ACTIVE,
      ash::LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);
  auto policy_provider = RestrictedMGSPolicyProvider::Create();
  ASSERT_TRUE(policy_provider);
  EXPECT_TRUE(expected_policy_bundle.Equals(policy_provider->policies()));
}

TEST_F(RestrictedMGSPolicyProviderAshTest,
       DeviceRestrictedManagedGuestSessionEnabled) {
  cros_settings_helper_->SetBoolean(
      ash::kDeviceRestrictedManagedGuestSessionEnabled, true);
  auto expected_policy_bundle = BuildRestrictedPolicyBundle();

  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LOGGED_IN_ACTIVE,
      ash::LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);
  auto policy_provider = RestrictedMGSPolicyProvider::Create();
  ASSERT_TRUE(policy_provider);
  EXPECT_TRUE(expected_policy_bundle->Equals(policy_provider->policies()));
}

}  // namespace policy

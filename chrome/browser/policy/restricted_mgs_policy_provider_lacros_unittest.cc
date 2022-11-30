// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/restricted_mgs_policy_provider.h"

#include <memory>

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/certificates_handler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "chromeos/startup/browser_init_params.h"
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

class RestrictedMGSPolicyProviderLacrosTest : public testing::Test {
 protected:
  void SetInitParams(crosapi::mojom::SessionType session_type,
                     crosapi::mojom::DeviceSettings_OptionalBool restricted) {
    auto params = crosapi::mojom::BrowserInitParams::New();
    params->session_type = session_type;
    params->device_settings = crosapi::mojom::DeviceSettings::New();
    params->device_settings->device_restricted_managed_guest_session_enabled =
        restricted;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(params));
  }
};

TEST_F(RestrictedMGSPolicyProviderLacrosTest,
       CreateRestrictedMGSPolicyProvider) {
  // Doesn't get created for a regular session.
  SetInitParams(
      /*session_type=*/crosapi::mojom::SessionType::kRegularSession,
      /*restricted=*/crosapi::mojom::DeviceSettings::OptionalBool::kUnset);
  auto policy_provider = RestrictedMGSPolicyProvider::Create();
  EXPECT_FALSE(policy_provider);

  // Gets created for a Managed Guest Session.
  SetInitParams(
      /*session_type=*/crosapi::mojom::SessionType::kPublicSession,
      /*restricted=*/crosapi::mojom::DeviceSettings::OptionalBool::kTrue);
  policy_provider = RestrictedMGSPolicyProvider::Create();
  EXPECT_TRUE(policy_provider);
}

TEST_F(RestrictedMGSPolicyProviderLacrosTest,
       DeviceRestrictedManagedGuestSessionDisabled) {
  SetInitParams(
      /*session_type=*/crosapi::mojom::SessionType::kPublicSession,
      /*restricted=*/crosapi::mojom::DeviceSettings::OptionalBool::kFalse);
  // Empty policy bundle.
  PolicyMap expected_policy_map;
  PolicyBundle expected_policy_bundle;
  expected_policy_bundle.Get(PolicyNamespace(
      POLICY_DOMAIN_CHROME, std::string())) = expected_policy_map.Clone();

  auto policy_provider = RestrictedMGSPolicyProvider::Create();
  ASSERT_TRUE(policy_provider);
  EXPECT_TRUE(expected_policy_bundle.Equals(policy_provider->policies()));
}

TEST_F(RestrictedMGSPolicyProviderLacrosTest,
       DeviceRestrictedManagedGuestSessionEnabled) {
  SetInitParams(
      /*session_type=*/crosapi::mojom::SessionType::kPublicSession,
      /*restricted=*/crosapi::mojom::DeviceSettings::OptionalBool::kTrue);
  auto expected_policy_bundle = BuildRestrictedPolicyBundle();

  auto policy_provider = RestrictedMGSPolicyProvider::Create();
  ASSERT_TRUE(policy_provider);
  EXPECT_TRUE(expected_policy_bundle->Equals(policy_provider->policies()));
}

}  // namespace policy

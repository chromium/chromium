// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/certificates_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/startup/browser_init_params.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

namespace {

const PolicyNamespace kChromeNamespace(POLICY_DOMAIN_CHROME, std::string());

PolicyMap BuildRestrictedPolicyMap() {
  PolicyMap policy_map;
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

class RestrictedMGSPolicyProviderLacrosBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<
          crosapi::mojom::DeviceSettings::OptionalBool> {
 public:
  void SetUp() override {
    // The value of DeviceRestrictedManagedGuestSessionEnabled is passed as a
    // test parameter.
    SetInitParams(
        /*session_type=*/crosapi::mojom::SessionType::kPublicSession,
        /*restricted=*/GetParam());
    InProcessBrowserTest::SetUp();
  }

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

INSTANTIATE_TEST_SUITE_P(
    All,
    RestrictedMGSPolicyProviderLacrosBrowserTest,
    testing::Values(crosapi::mojom::DeviceSettings::OptionalBool::kFalse,
                    crosapi::mojom::DeviceSettings::OptionalBool::kTrue));

IN_PROC_BROWSER_TEST_P(RestrictedMGSPolicyProviderLacrosBrowserTest,
                       DeviceRestrictedManagedGuestSessionEnabled) {
  auto* profile = ProfileManager::GetPrimaryUserProfile();
  auto* policy_connector = profile->GetProfilePolicyConnector();
  const PolicyMap& current_policy_map =
      policy_connector->policy_service()->GetPolicies(kChromeNamespace);

  PolicyMap expected_policy_map;
  if (GetParam() == crosapi::mojom::DeviceSettings::OptionalBool::kTrue)
    expected_policy_map = BuildRestrictedPolicyMap();

  EXPECT_TRUE(expected_policy_map.Equals(current_policy_map));
}

}  // namespace policy

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/foundation_util.h"
#include "base/values.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/mac/app_mode_common.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class AdHocCodeSigningForPWAsEnabledTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<std::tuple<
          /*features::kUseAdHocSigningForWebAppShims=*/bool,
          /*policy::key::kUseAdHocCodeSigningForPWAsEnabled=*/std::optional<
              bool>>> {
 public:
  // InProcessBrowserTest implementation:
  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    policy::PolicyMap values;
    if (GetPolicyValue().has_value()) {
      values.Set(policy::key::kAdHocCodeSigningForPWAsEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD, base::Value(*GetPolicyValue()),
                 nullptr);
    }
    policy_provider_.UpdateChromePolicy(values);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    feature_list_.InitWithFeatureState(features::kUseAdHocSigningForWebAppShims,
                                       GetFeatureValue());
    override_registration_ =
        web_app::OsIntegrationTestOverrideImpl::OverrideForTesting();
    destination_dir_ =
        override_registration_->test_override().chrome_apps_folder();

    InProcessBrowserTest::SetUp();
  }

 protected:
  bool GetFeatureValue() const { return std::get<0>(GetParam()); }
  std::optional<bool> GetPolicyValue() const { return std::get<1>(GetParam()); }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<web_app::OsIntegrationTestOverrideImpl::BlockingRegistration>
      override_registration_;
  base::FilePath destination_dir_;
};

// TODO(crbug.com/369346087): Deflake and re-enable.
IN_PROC_BROWSER_TEST_P(AdHocCodeSigningForPWAsEnabledTest,
                       DISABLED_IsRespected) {
  webapps::AppId app_id = web_app::test::InstallDummyWebApp(
      browser()->profile(), "Example", GURL("https://www.example.com"));

  base::FilePath info_plist_path = destination_dir_.Append("Example.app")
                                       .Append("Contents")
                                       .Append("Info.plist");
  NSDictionary* infoPlist =
      [NSDictionary dictionaryWithContentsOfURL:base::apple::FilePathToNSURL(
                                                    info_plist_path)];
  ASSERT_TRUE(infoPlist);
  bool is_ad_hoc_signed =
      [infoPlist[app_mode::kCrAppModeIsAdHocSignedKey] boolValue];

  // If the feature is disabled, the policy can never override it.
  if (!GetFeatureValue()) {
    ASSERT_FALSE(is_ad_hoc_signed);
  } else {
    // When the feature is enabled, the policy is respected.
    // An unset policy respects the feature flag.
    ASSERT_EQ(is_ad_hoc_signed, GetPolicyValue().value_or(true));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AdHocCodeSigningForPWAsEnabledTest,
    ::testing::Combine(::testing::Values(false, true),
                       ::testing::Values(false, true, std::nullopt)));

}  // namespace policy

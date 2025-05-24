// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <optional>
#include <string>

#include "base/check_deref.h"
#include "base/test/gtest_tags.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/app_mode/test/network_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_server_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/key_pair.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::BlockKioskLaunch;
using kiosk::test::LaunchAppManually;
using kiosk::test::TheKioskApp;
using kiosk::test::WaitKioskLaunched;
using kiosk::test::WaitNetworkScreen;

namespace {

// Workflow: COM_KIOSK_CUJ3_TASK8_WF1.
constexpr char kLaunchKioskOfflineTag[] =
    "screenplay-35e430a3-04b3-46a7-aa0a-207a368b8cba";

const web_package::SignedWebBundleId kTestWebBundleId =
    web_app::test::GetDefaultEd25519WebBundleId();
const web_package::test::KeyPair kTestKeyPair =
    web_app::test::GetDefaultEd25519KeyPair();

// Possible values for `KioskWebAppOfflineEnabledTest`.
enum class TestAppType { kKioskWeb, kKioskIwa };

std::string TestAppName(const testing::TestParamInfo<TestAppType>& info) {
  switch (info.param) {
    case TestAppType::kKioskWeb:
      return "KioskWeb";
    case TestAppType::kKioskIwa:
      return "KioskIwa";
  }
}

void ExpectNetworkScreenContinueButtonNotShown() {
  const test::UIPath kNetworkConfigureScreenContinueButton = {"error-message",
                                                              "continueButton"};
  test::OobeJS().ExpectPathDisplayed(
      /*displayed=*/false,
      /*element_id=*/kNetworkConfigureScreenContinueButton);
}

}  // namespace

// Verifies that `KioskWebAppOfflineEnabled` policy works correctly for
// supported app types.
template <bool policy_value>
class KioskWebAppOfflineEnabledTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<TestAppType> {
 public:
  KioskWebAppOfflineEnabledTest() {
    iwa_server_mixin_.AddBundle(
        web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder())
            .BuildBundle(kTestKeyPair));
  }

  KioskWebAppOfflineEnabledTest(const KioskWebAppOfflineEnabledTest&) = delete;
  KioskWebAppOfflineEnabledTest& operator=(
      const KioskWebAppOfflineEnabledTest&) = delete;

  ~KioskWebAppOfflineEnabledTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);

    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    policy::PolicyMap values;
    values.Set(policy::key::kKioskWebAppOfflineEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(policy_value), nullptr);
    provider_.UpdateChromePolicy(values);
  }

 protected:
  KioskMixin::Config GetConfig() {
    switch (GetParam()) {
      case TestAppType::kKioskWeb:
        return {/*name=*/"WebApp",
                /*auto_launch_account_id=*/{},
                {KioskMixin::SimpleWebAppOption()}};

      case TestAppType::kKioskIwa:
        return {/*name=*/"IsolatedWebApp",
                /*auto_launch_account_id=*/{},
                {KioskMixin::IsolatedWebAppOption(
                    /*account_id=*/"simple-iwa@localhost",
                    /*web_bundle_id=*/kTestWebBundleId,
                    /*update_manifest_url=*/
                    iwa_server_mixin_.GetUpdateManifestUrl(kTestWebBundleId))}};
    }
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  web_app::IsolatedWebAppUpdateServerMixin iwa_server_mixin_{&mixin_host_};
  NetworkStateMixin network_state_{&mixin_host_};
  KioskMixin kiosk_{&mixin_host_, /*cached_configuration=*/GetConfig()};
};

using KioskWebAppOfflineDisabledByPolicyTest =
    KioskWebAppOfflineEnabledTest<false>;

IN_PROC_BROWSER_TEST_P(KioskWebAppOfflineDisabledByPolicyTest,
                       PRE_CannotLaunchOffline) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  ASSERT_TRUE(WaitKioskLaunched());
}

IN_PROC_BROWSER_TEST_P(KioskWebAppOfflineDisabledByPolicyTest,
                       CannotLaunchOffline) {
  base::AddFeatureIdTagToTestResult(kLaunchKioskOfflineTag);

  network_state_.SimulateOffline();
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));

  auto scoped_launch_blocker = BlockKioskLaunch();
  WaitNetworkScreen();
  ExpectNetworkScreenContinueButtonNotShown();

  scoped_launch_blocker.reset();
  network_state_.SimulateOnline();
  ASSERT_TRUE(WaitKioskLaunched());
}

INSTANTIATE_TEST_SUITE_P(All,
                         KioskWebAppOfflineDisabledByPolicyTest,
                         testing::Values(TestAppType::kKioskWeb,
                                         TestAppType::kKioskIwa),
                         TestAppName);

using KioskWebAppOfflineEnabledByPolicyTest =
    KioskWebAppOfflineEnabledTest<true>;

IN_PROC_BROWSER_TEST_P(KioskWebAppOfflineEnabledByPolicyTest,
                       PRE_LaunchesOffline) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  ASSERT_TRUE(WaitKioskLaunched());
}

IN_PROC_BROWSER_TEST_P(KioskWebAppOfflineEnabledByPolicyTest, LaunchesOffline) {
  base::AddFeatureIdTagToTestResult(kLaunchKioskOfflineTag);

  network_state_.SimulateOffline();
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  ASSERT_TRUE(WaitKioskLaunched());
}

INSTANTIATE_TEST_SUITE_P(All,
                         KioskWebAppOfflineEnabledByPolicyTest,
                         testing::Values(TestAppType::kKioskWeb,
                                         TestAppType::kKioskIwa),
                         TestAppName);

}  // namespace ash

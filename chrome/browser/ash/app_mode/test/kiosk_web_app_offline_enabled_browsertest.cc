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
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::BlockKioskLaunch;
using kiosk::test::TheKioskWebApp;
using kiosk::test::WaitNetworkScreen;

namespace {

// Workflow: COM_KIOSK_CUJ3_TASK8_WF1.
constexpr char kLaunchKioskOfflineTag[] =
    "screenplay-35e430a3-04b3-46a7-aa0a-207a368b8cba";

void ExpectNetworkScreenContinueButtonNotShown() {
  const test::UIPath kNetworkConfigureScreenContinueButton = {"error-message",
                                                              "continueButton"};
  test::OobeJS().ExpectPathDisplayed(
      /*displayed=*/false,
      /*element_id=*/kNetworkConfigureScreenContinueButton);
}

}  // namespace

// Verifies the `KioskWebAppOfflineEnabled` policy works correctly.
template <bool policy_value>
class KioskWebAppOfflineEnabledTest : public MixinBasedInProcessBrowserTest {
 public:
  KioskWebAppOfflineEnabledTest() = default;

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

  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;

  NetworkStateMixin network_state_{&mixin_host_};

  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/KioskMixin::Config{
                        /*name=*/{},
                        /*auto_launch_account_id=*/{},
                        {KioskMixin::SimpleWebAppOption()}}};
};

using KioskWebAppOfflineDisabledByPolicyTest =
    KioskWebAppOfflineEnabledTest<false>;

using KioskWebAppOfflineEnabledByPolicyTest =
    KioskWebAppOfflineEnabledTest<true>;

IN_PROC_BROWSER_TEST_F(KioskWebAppOfflineDisabledByPolicyTest,
                       PRE_CannotLaunchOffline) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskWebApp()));
  ASSERT_TRUE(kiosk_.WaitSessionLaunched());
}

IN_PROC_BROWSER_TEST_F(KioskWebAppOfflineDisabledByPolicyTest,
                       CannotLaunchOffline) {
  base::AddFeatureIdTagToTestResult(kLaunchKioskOfflineTag);

  network_state_.SimulateOffline();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskWebApp()));

  auto scoped_launch_blocker = BlockKioskLaunch();
  WaitNetworkScreen();
  ExpectNetworkScreenContinueButtonNotShown();

  scoped_launch_blocker.reset();
  network_state_.SimulateOnline();
  ASSERT_TRUE(kiosk_.WaitSessionLaunched());
}

IN_PROC_BROWSER_TEST_F(KioskWebAppOfflineEnabledByPolicyTest,
                       PRE_LaunchesOffline) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskWebApp()));
  ASSERT_TRUE(kiosk_.WaitSessionLaunched());
}

IN_PROC_BROWSER_TEST_F(KioskWebAppOfflineEnabledByPolicyTest, LaunchesOffline) {
  base::AddFeatureIdTagToTestResult(kLaunchKioskOfflineTag);

  network_state_.SimulateOffline();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskWebApp()));
  ASSERT_TRUE(kiosk_.WaitSessionLaunched());
}

}  // namespace ash

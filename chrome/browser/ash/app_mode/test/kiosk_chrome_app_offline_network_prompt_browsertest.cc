// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/app_mode/test/fake_cws_chrome_apps.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/app_mode/test/network_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

using kiosk::test::AutoLaunchKioskApp;
using kiosk::test::CloseAppWindow;
using kiosk::test::CurrentProfile;
using kiosk::test::IsAppInstalled;
using kiosk::test::OfflineEnabledChromeAppV1;
using kiosk::test::TheKioskApp;
using kiosk::test::WaitKioskLaunched;
using kiosk::test::WaitNetworkScreen;
using kiosk::test::WaitSplashScreen;

namespace {

// Values of the `DeviceLocalAccountPromptForNetworkWhenOffline` policy.
enum class PolicyValue { kUnset, kDisabled, kEnabled };

void SetPolicyValue(ScopedDevicePolicyUpdate& scoped_update,
                    PolicyValue policy_value) {
  CHECK_NE(policy_value, PolicyValue::kUnset);
  bool enable = policy_value == PolicyValue::kEnabled;
  scoped_update.policy_payload()
      ->mutable_device_local_accounts()
      ->set_prompt_for_network_when_offline(enable);
}

void WaitNetworkTimeoutMessage() {
  test::TestPredicateWaiter(base::BindRepeating([] {
    const test::UIPath kSplashScreenLaunchText = {"app-launch-splash",
                                                  "launchText"};
    return test::OobeJS().GetString(
               ash::test::GetOobeElementPath(kSplashScreenLaunchText) +
               ".textContent") ==
           l10n_util::GetStringUTF8(IDS_APP_START_NETWORK_WAIT_TIMEOUT_MESSAGE);
  })).Wait();
}

std::string TestParamName(const testing::TestParamInfo<PolicyValue>& info) {
  switch (info.param) {
    case PolicyValue::kUnset:
      return "PolicyUnset";
    case PolicyValue::kDisabled:
      return "PolicyDisabled";
    case PolicyValue::kEnabled:
      return "PolicyEnabled";
  }
}

}  // namespace

// Verifies the `DeviceLocalAccountPromptForNetworkWhenOffline` policy works in
// Chrome apps in Kiosk.
class KioskChromeAppOfflineNetworkPromptTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<PolicyValue> {
 public:
  KioskChromeAppOfflineNetworkPromptTest() = default;
  KioskChromeAppOfflineNetworkPromptTest(
      const KioskChromeAppOfflineNetworkPromptTest&) = delete;
  KioskChromeAppOfflineNetworkPromptTest& operator=(
      const KioskChromeAppOfflineNetworkPromptTest&) = delete;

  ~KioskChromeAppOfflineNetworkPromptTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    if (GetParam() != PolicyValue::kUnset) {
      SetPolicyValue(*kiosk_.device_state_mixin().RequestDevicePolicyUpdate(),
                     GetParam());
    }
  }

  NetworkStateMixin network_state_{&mixin_host_};

  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/KioskMixin::Config{
                        /*name=*/{},
                        KioskMixin::AutoLaunchAccount{
                            KioskMixin::SimpleChromeAppOption().account_id},
                        /*options=*/{KioskMixin::SimpleChromeAppOption()}}};
};

// Alias to test policy values `kUnset` and `kEnabled`.
using KioskChromeAppOfflineNetworkPromptEnabledTest =
    KioskChromeAppOfflineNetworkPromptTest;

IN_PROC_BROWSER_TEST_P(KioskChromeAppOfflineNetworkPromptEnabledTest,
                       NetworkScreenAppears) {
  network_state_.SimulateOffline();

  WaitNetworkScreen();

  network_state_.SimulateOnline();
  ASSERT_TRUE(WaitKioskLaunched());
}

INSTANTIATE_TEST_SUITE_P(All,
                         KioskChromeAppOfflineNetworkPromptEnabledTest,
                         testing::Values(PolicyValue::kUnset,
                                         PolicyValue::kEnabled),
                         TestParamName);

// Alias to test policy value `kDisabled`.
using KioskChromeAppOfflineNetworkPromptDisabledTest =
    KioskChromeAppOfflineNetworkPromptTest;

IN_PROC_BROWSER_TEST_P(KioskChromeAppOfflineNetworkPromptDisabledTest,
                       NetworkScreenDoesNotAppear) {
  network_state_.SimulateOffline();

  const test::UIPath kConfigNetworkLink = {"app-launch-splash",
                                           "configNetwork"};

  WaitSplashScreen();
  WaitNetworkTimeoutMessage();
  test::OobeJS().ExpectHiddenPath(kConfigNetworkLink);

  network_state_.SimulateOnline();
  ASSERT_TRUE(WaitKioskLaunched());
}

INSTANTIATE_TEST_SUITE_P(All,
                         KioskChromeAppOfflineNetworkPromptDisabledTest,
                         testing::Values(PolicyValue::kDisabled),
                         TestParamName);

}  // namespace ash

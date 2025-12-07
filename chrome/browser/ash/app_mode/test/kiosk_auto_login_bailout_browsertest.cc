// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

#include "base/auto_reset.h"
#include "base/check_deref.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::BlockKioskLaunch;
using kiosk::test::PressBailoutAccelerator;
using kiosk::test::TheKioskApp;
using kiosk::test::WaitSplashScreen;

namespace {

// The possible values of the DeviceLocalAccountAutoLoginBailoutEnabled policy.
enum class PolicyValue { kUnset, kEnabled, kDisabled };

std::string_view ToString(PolicyValue value) {
  switch (value) {
    case PolicyValue::kUnset:
      return "Unset";
    case PolicyValue::kEnabled:
      return "Enabled";
    case PolicyValue::kDisabled:
      return "Disabled";
  }
}

void CachePolicyValue(std::unique_ptr<ScopedDevicePolicyUpdate> update,
                      PolicyValue policy_value) {
  if (policy_value == PolicyValue::kUnset) {
    return;
  }
  update->policy_payload()
      ->mutable_device_local_accounts()
      ->set_enable_auto_login_bailout(policy_value == PolicyValue::kEnabled);
}

std::string ParamName(
    const testing::TestParamInfo<std::tuple<KioskMixin::Config, PolicyValue>>&
        info) {
  const auto& [config, policy_value] = info.param;
  auto name = config.name.value_or(base::StringPrintf("%zu", info.index));
  return base::StrCat({name, "WithPolicy", ToString(policy_value)});
}

}  // namespace

// Verifies the "DeviceLocalAccountAutoLoginBailoutEnabled" policy in Kiosk.
class KioskAutoLoginBailoutTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<
          std::tuple<KioskMixin::Config, PolicyValue>> {
 public:
  KioskAutoLoginBailoutTest() = default;

  KioskAutoLoginBailoutTest(const KioskAutoLoginBailoutTest&) = delete;
  KioskAutoLoginBailoutTest& operator=(const KioskAutoLoginBailoutTest&) =
      delete;

  ~KioskAutoLoginBailoutTest() override = default;

  const KioskMixin::Config& config() { return std::get<0>(GetParam()); }

  PolicyValue policy_value() { return std::get<1>(GetParam()); }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    CachePolicyValue(kiosk_.device_state_mixin().RequestDevicePolicyUpdate(),
                     policy_value());
  }

  // Block Kiosk launch early enough since the app will auto launch.
  std::optional<base::AutoReset<bool>> scoped_launch_blocker_ =
      BlockKioskLaunch();

  KioskMixin kiosk_{&mixin_host_, /*cached_configuration=*/config()};
};

// Alias to test policy values `kUnset` and `kEnabled`.
using KioskAutoLoginBailoutEnabledTest = KioskAutoLoginBailoutTest;

IN_PROC_BROWSER_TEST_P(KioskAutoLoginBailoutEnabledTest,
                       AcceleratorCancelsLaunch) {
  WaitSplashScreen();

  ASSERT_TRUE(PressBailoutAccelerator());

  RunUntilBrowserProcessQuits();
  EXPECT_EQ(
      KioskAppLaunchError::Error::kUserCancel,
      KioskAppLaunchError::Get(CHECK_DEREF(g_browser_process->local_state())));
  EXPECT_FALSE(KioskController::Get().IsSessionStarting());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskAutoLoginBailoutEnabledTest,
    testing::Combine(
        // The accelerator only works in auto launch Kiosk.
        testing::ValuesIn(KioskMixin::ConfigsToAutoLaunchEachAppType()),
        testing::Values(PolicyValue::kUnset, PolicyValue::kEnabled)),
    ParamName);

// Alias to test policy value `kDisabled`.
using KioskAutoLoginBailoutDisabledTest = KioskAutoLoginBailoutTest;

IN_PROC_BROWSER_TEST_P(KioskAutoLoginBailoutDisabledTest,
                       AcceleratorDoesNotCancelLaunch) {
  WaitSplashScreen();

  ASSERT_TRUE(PressBailoutAccelerator());

  // If the bailout is enabled `KioskController` halts the launch and destroys
  // `KioskLaunchController` asynchronously, which causes `IsSessionStarting` to
  // become false. `RunUntilIdle` here to verify the destruction did not happen
  // and `IsSessionStarting` remains true.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(KioskController::Get().IsSessionStarting());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskAutoLoginBailoutDisabledTest,
    testing::Combine(
        testing::ValuesIn(KioskMixin::ConfigsToAutoLaunchEachAppType()),
        testing::Values(PolicyValue::kDisabled)),
    ParamName);

}  // namespace ash

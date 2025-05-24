// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <string>
#include <string_view>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/check_deref.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/load_profile.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_app_install_util.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_closed_waiter.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::CachePolicy;
using kiosk::test::CreatePopupBrowser;
using kiosk::test::CreateRegularBrowser;
using kiosk::test::CurrentProfile;
using kiosk::test::DidKioskCloseNewWindow;
using kiosk::test::TheKioskWebApp;
using kiosk::test::WaitKioskLaunched;

namespace {

bool GetPolicyValueInPrefs(Profile& profile) {
  return profile.GetPrefs()->GetBoolean(prefs::kNewWindowsInKioskAllowed);
}

// Returns the app name for the given web `app`. This is the name
// `KioskBrowserWindowHandler` uses to determine whether to close a window.
std::string WebAppWindowName(const KioskApp& app) {
  auto [_, app_id] =
      chromeos::GetKioskWebAppInstallState(CurrentProfile(), app.url().value());
  return web_app::GenerateApplicationNameFromAppId(app_id.value());
}

KioskMixin::DefaultServerWebAppOption WebAppInConfig() {
  return KioskMixin::SimpleWebAppOption();
}

KioskMixin::Config Config() {
  return KioskMixin::Config{
      /*name=*/{},
      KioskMixin::AutoLaunchAccount{WebAppInConfig().account_id},
      {WebAppInConfig()}};
}

// The possible values the `NewWindowsInKioskAllowed` policy can be in.
enum class PolicyValue { kUnset, kTrue, kFalse };

// Caches the given `policy_value` in fake session manager for `account_id`.
void CachePolicyValue(const std::string& account_id, PolicyValue policy_value) {
  if (policy_value == PolicyValue::kUnset) {
    return;
  }

  CachePolicy(account_id, [policy_value](policy::UserPolicyBuilder& builder) {
    builder.payload().mutable_newwindowsinkioskallowed()->set_value(
        policy_value == PolicyValue::kTrue);
  });
}

std::string PolicyValueName(const testing::TestParamInfo<PolicyValue>& info) {
  switch (info.param) {
    case PolicyValue::kUnset:
      return "PolicyUnset";
    case PolicyValue::kTrue:
      return "PolicyTrue";
    case PolicyValue::kFalse:
      return "PolicyFalse";
  }
}

}  // namespace

// Verifies the `NewWindowsInKioskAllowed` policy when it is set to true.
class NewWindowsInKioskAllowedTest : public MixinBasedInProcessBrowserTest {
 public:
  NewWindowsInKioskAllowedTest() = default;
  NewWindowsInKioskAllowedTest(const NewWindowsInKioskAllowedTest&) = delete;
  NewWindowsInKioskAllowedTest& operator=(const NewWindowsInKioskAllowedTest&) =
      delete;
  ~NewWindowsInKioskAllowedTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    CachePolicyValue(WebAppInConfig().account_id, PolicyValue::kTrue);
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(WaitKioskLaunched());
  }

  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/Config()};
};

IN_PROC_BROWSER_TEST_F(NewWindowsInKioskAllowedTest, AllowsNewPopupWindows) {
  auto& profile = CurrentProfile();
  ASSERT_TRUE(GetPolicyValueInPrefs(profile));

  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  Browser& initial_browser = CHECK_DEREF(BrowserList::GetInstance()->get(0));

  Browser& popup =
      CreatePopupBrowser(profile, WebAppWindowName(TheKioskWebApp()));

  ASSERT_FALSE(DidKioskCloseNewWindow());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);

  EXPECT_FALSE(initial_browser.GetBrowserView().CanUserEnterFullscreen());
  EXPECT_FALSE(popup.GetBrowserView().CanUserEnterFullscreen());
  EXPECT_TRUE(initial_browser.GetBrowserView().IsFullscreen());
  EXPECT_TRUE(popup.GetBrowserView().IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(NewWindowsInKioskAllowedTest,
                       DisallowsNewRegularWindows) {
  ASSERT_TRUE(GetPolicyValueInPrefs(CurrentProfile()));

  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  Browser& browser = CreateRegularBrowser(CurrentProfile());
  ASSERT_TRUE(TestBrowserClosedWaiter(&browser).WaitUntilClosed());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
}

// Verifies the `NewWindowsInKioskAllowed` policy when it is unset or false.
class NewWindowsInKioskDisallowedTest
    : public NewWindowsInKioskAllowedTest,
      public testing::WithParamInterface<PolicyValue> {
 public:
  NewWindowsInKioskDisallowedTest() = default;
  NewWindowsInKioskDisallowedTest(const NewWindowsInKioskDisallowedTest&) =
      delete;
  NewWindowsInKioskDisallowedTest& operator=(
      const NewWindowsInKioskDisallowedTest&) = delete;
  ~NewWindowsInKioskDisallowedTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    CachePolicyValue(WebAppInConfig().account_id, /*policy_value=*/GetParam());
  }
};

IN_PROC_BROWSER_TEST_P(NewWindowsInKioskDisallowedTest, DisallowsNewWindows) {
  ASSERT_FALSE(GetPolicyValueInPrefs(CurrentProfile()));

  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);

  Browser& browser = CreateRegularBrowser(CurrentProfile());
  ASSERT_TRUE(TestBrowserClosedWaiter(&browser).WaitUntilClosed());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);

  Browser& popup =
      CreatePopupBrowser(CurrentProfile(), WebAppWindowName(TheKioskWebApp()));
  ASSERT_TRUE(TestBrowserClosedWaiter(&popup).WaitUntilClosed());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
}

INSTANTIATE_TEST_SUITE_P(All,
                         NewWindowsInKioskDisallowedTest,
                         testing::Values(PolicyValue::kUnset,
                                         PolicyValue::kFalse),
                         PolicyValueName);

}  // namespace ash

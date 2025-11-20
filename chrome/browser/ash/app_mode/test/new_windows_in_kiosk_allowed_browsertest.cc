// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <tuple>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/check_deref.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/load_profile.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/app_mode/web_app/kiosk_web_app_manager.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_app_install_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/test/test_browser_closed_waiter.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::CachePolicy;
using kiosk::test::CreatePopupBrowser;
using kiosk::test::CreateRegularBrowser;
using kiosk::test::CurrentProfile;
using kiosk::test::DidKioskCloseNewWindow;
using kiosk::test::DidKioskHideNewWindow;
using kiosk::test::TheKioskWebApp;
using kiosk::test::WaitKioskLaunched;

namespace {

constexpr const char* kTestUrlParams[] = {"", "https://www.test.com"};

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

std::string UrlValueName(const std::string& url) {
  return url.empty() ? "NavigateToEmptyUrl" : "NavigateToNonEmptyUrl";
}

std::string PolicyValueName(PolicyValue policy_value) {
  switch (policy_value) {
    case PolicyValue::kUnset:
      return "WithPolicyUnset";
    case PolicyValue::kTrue:
      return "WithPolicyTrue";
    case PolicyValue::kFalse:
      return "WithPolicyFalse";
  }
}

std::string TestUrlName(const testing::TestParamInfo<const char*>& info) {
  const auto& url = info.param;
  return UrlValueName(url);
}

std::string TestValueName(
    const testing::TestParamInfo<std::tuple<PolicyValue, const char*>>& info) {
  const auto& [policy_value, url] = info.param;
  return base::StrCat({UrlValueName(url), PolicyValueName(policy_value)});
}

size_t VisibleBrowserCount() {
  auto visible_browsers =
      ui_test_utils::FindMatchingBrowsers([](BrowserWindowInterface* browser) {
        return browser->GetWindow() && browser->GetWindow()->IsVisible();
      });
  return visible_browsers.size();
}

}  // namespace

// Base class to test new windows in kiosk.
// Note: `NewWindowsInKioskAllowed` policy is unset.
class NewWindowsInKioskTest : public MixinBasedInProcessBrowserTest {
 public:
  NewWindowsInKioskTest() = default;
  NewWindowsInKioskTest(const NewWindowsInKioskTest&) = delete;
  NewWindowsInKioskTest& operator=(const NewWindowsInKioskTest&) = delete;
  ~NewWindowsInKioskTest() override = default;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(WaitKioskLaunched());
  }

  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/Config()};
};

// Verifies the `NewWindowsInKioskAllowed` policy when it is set to true.
class NewWindowsInKioskAllowedTest
    : public NewWindowsInKioskTest,
      public ::testing::WithParamInterface<const char*> {
 public:
  NewWindowsInKioskAllowedTest() = default;
  NewWindowsInKioskAllowedTest(const NewWindowsInKioskAllowedTest&) = delete;
  NewWindowsInKioskAllowedTest& operator=(const NewWindowsInKioskAllowedTest&) =
      delete;
  ~NewWindowsInKioskAllowedTest() override = default;

  const GURL url() const { return GURL(GetParam()); }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    CachePolicyValue(WebAppInConfig().account_id, PolicyValue::kTrue);
  }
};

IN_PROC_BROWSER_TEST_F(NewWindowsInKioskAllowedTest, CloseBrowserIfReOpen) {
  ASSERT_TRUE(GetPolicyValueInPrefs(CurrentProfile()));

  EXPECT_EQ(VisibleBrowserCount(), 1u);
  Browser& browser = CreateRegularBrowser(CurrentProfile(), GURL());
  ASSERT_TRUE(DidKioskHideNewWindow(&browser));
  EXPECT_EQ(VisibleBrowserCount(), 1u);
  ASSERT_FALSE(browser.window()->IsVisible());
  browser.window()->Show();
  ASSERT_TRUE(TestBrowserClosedWaiter(&browser).WaitUntilClosed());
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1u);
}

IN_PROC_BROWSER_TEST_P(NewWindowsInKioskAllowedTest, AllowsNewPopupWindows) {
  auto& profile = CurrentProfile();
  ASSERT_TRUE(GetPolicyValueInPrefs(profile));

  EXPECT_EQ(VisibleBrowserCount(), 1u);
  Browser& initial_browser =
      CHECK_DEREF(GetLastActiveBrowserWindowInterfaceWithAnyProfile()
                      ->GetBrowserForMigrationOnly());

  Browser& popup =
      CreatePopupBrowser(profile, WebAppWindowName(TheKioskWebApp()), url());

  ASSERT_FALSE(DidKioskCloseNewWindow());
  EXPECT_EQ(VisibleBrowserCount(), 2u);

  EXPECT_FALSE(initial_browser.GetBrowserView()
                   .GetExclusiveAccessContext()
                   ->CanUserEnterFullscreen());
  EXPECT_FALSE(popup.GetBrowserView()
                   .GetExclusiveAccessContext()
                   ->CanUserEnterFullscreen());
  EXPECT_TRUE(initial_browser.GetBrowserView().IsFullscreen());
  EXPECT_TRUE(popup.GetBrowserView().IsFullscreen());
}

IN_PROC_BROWSER_TEST_P(NewWindowsInKioskAllowedTest,
                       DisallowsNewRegularWindows) {
  ASSERT_TRUE(GetPolicyValueInPrefs(CurrentProfile()));

  EXPECT_EQ(VisibleBrowserCount(), 1u);
  Browser& browser = CreateRegularBrowser(CurrentProfile(), url());
  ASSERT_TRUE(DidKioskHideNewWindow(&browser));
  EXPECT_EQ(VisibleBrowserCount(), 1u);
}

INSTANTIATE_TEST_SUITE_P(All,
                         NewWindowsInKioskAllowedTest,
                         testing::ValuesIn(kTestUrlParams),
                         TestUrlName);

// Verifies the `NewWindowsInKioskAllowed` policy when it is unset or false.
class NewWindowsInKioskDisallowedTest
    : public NewWindowsInKioskTest,
      public testing::WithParamInterface<std::tuple<PolicyValue, const char*>> {
 public:
  NewWindowsInKioskDisallowedTest() = default;
  NewWindowsInKioskDisallowedTest(const NewWindowsInKioskDisallowedTest&) =
      delete;
  NewWindowsInKioskDisallowedTest& operator=(
      const NewWindowsInKioskDisallowedTest&) = delete;
  ~NewWindowsInKioskDisallowedTest() override = default;

  PolicyValue policy_value() const { return std::get<0>(GetParam()); }

  const GURL url() const { return GURL(std::get<1>(GetParam())); }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    CachePolicyValue(WebAppInConfig().account_id, policy_value());
  }
};

IN_PROC_BROWSER_TEST_P(NewWindowsInKioskDisallowedTest,
                       HidesNewRegularBrowserWindows) {
  ASSERT_FALSE(GetPolicyValueInPrefs(CurrentProfile()));

  EXPECT_EQ(VisibleBrowserCount(), 1u);

  Browser& browser = CreateRegularBrowser(CurrentProfile(), url());
  ASSERT_TRUE(DidKioskHideNewWindow(&browser));
  EXPECT_EQ(VisibleBrowserCount(), 1u);
}

IN_PROC_BROWSER_TEST_P(NewWindowsInKioskDisallowedTest,
                       HidesNewAppBrowserWindows) {
  ASSERT_FALSE(GetPolicyValueInPrefs(CurrentProfile()));

  EXPECT_EQ(VisibleBrowserCount(), 1u);

  Browser& popup = CreatePopupBrowser(
      CurrentProfile(), WebAppWindowName(TheKioskWebApp()), url());
  ASSERT_TRUE(DidKioskHideNewWindow(&popup));
  EXPECT_EQ(VisibleBrowserCount(), 1u);
}

INSTANTIATE_TEST_SUITE_P(All,
                         NewWindowsInKioskDisallowedTest,
                         testing::Combine(testing::Values(PolicyValue::kUnset,
                                                          PolicyValue::kFalse),
                                          testing::ValuesIn(kTestUrlParams)),
                         TestValueName);

}  // namespace ash

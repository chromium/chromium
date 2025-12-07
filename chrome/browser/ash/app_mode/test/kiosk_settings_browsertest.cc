// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/chromeos/app_mode/kiosk_settings_navigation_throttle.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace ash {

using chromeos::KioskSettingsNavigationThrottle;
using kiosk::test::WaitKioskLaunched;

namespace {

constexpr std::string_view kSettingsUrl = "https://settings.com";

using kiosk::test::CurrentProfile;
using kiosk::test::DidKioskCloseNewWindow;

KioskSystemSession& GetKioskSystemSession() {
  return CHECK_DEREF(KioskController::Get().GetKioskSystemSession());
}

content::WebContents& ActiveWebContents(Browser& browser) {
  content::WebContents& web_contents =
      CHECK_DEREF(browser.tab_strip_model()->GetActiveWebContents());
  return web_contents;
}

NavigateParams NavigateAndReturnParams(const GURL& url,
                                       WindowOpenDisposition disposition) {
  auto& profile = CurrentProfile();
  NavigateParams params(&profile, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = disposition;
  params.window_action = NavigateParams::WindowAction::kShowWindow;
  Navigate(&params);
  return params;
}

// Opens a popup at `url` and returns true if the window wasn't force closed.
bool OpenPopup(const GURL& url) {
  NavigateAndReturnParams(url, WindowOpenDisposition::NEW_POPUP);
  return !DidKioskCloseNewWindow();
}

// Navigates to `url` in the current tab, and returns the browser.
Browser& NavigateInCurrentTab(const GURL& url) {
  auto params =
      NavigateAndReturnParams(url, WindowOpenDisposition::CURRENT_TAB);
  CHECK(params.browser);
  return CHECK_DEREF(params.browser->GetBrowserForMigrationOnly());
}

GURL NavigateInBrowser(Browser& browser, const GURL& url) {
  auto& web_contents = ActiveWebContents(browser);
  NavigateToURLBlockUntilNavigationsComplete(
      /*web_contents=*/&web_contents, url,
      /*number_of_navigations=*/1,
      /*ignore_uncommitted_navigations=*/false);
  return web_contents.GetLastCommittedURL();
}

GURL NextCommittedUrl(Browser& browser) {
  auto& web_contents = ActiveWebContents(browser);
  content::TestNavigationObserver(&web_contents,
                                  /*expected_number_of_navigations=*/1)
      .Wait();
  return web_contents.GetLastCommittedURL();
}

// Helper for tests to override the list of settings pages.
class ScopedSettingsPages {
 public:
  explicit ScopedSettingsPages(
      std::vector<chromeos::KioskSettingsNavigationThrottle::SettingsPage>
          pages)
      : pages_(std::move(pages)) {
    chromeos::KioskSettingsNavigationThrottle::SetSettingPagesForTesting(
        &pages_);
  }

  ScopedSettingsPages(const ScopedSettingsPages&) = delete;
  ScopedSettingsPages& operator=(const ScopedSettingsPages&) = delete;

  ~ScopedSettingsPages() {
    chromeos::KioskSettingsNavigationThrottle::SetSettingPagesForTesting(
        nullptr);
  }

 private:
  std::vector<chromeos::KioskSettingsNavigationThrottle::SettingsPage> pages_;
};

}  // namespace

// Verifies settings pages work correctly in Kiosk.
class KioskSettingsTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<KioskMixin::Config> {
 public:
  KioskSettingsTest() = default;

  KioskSettingsTest(const KioskSettingsTest&) = delete;
  KioskSettingsTest& operator=(const KioskSettingsTest&) = delete;

  ~KioskSettingsTest() override = default;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(WaitKioskLaunched());
  }

  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/GetParam()};
};

IN_PROC_BROWSER_TEST_P(KioskSettingsTest, CanNavigateToSettingsUrl) {
  const GURL settings_url(kSettingsUrl);

  ScopedSettingsPages scoped_pages({
      {/*url=*/settings_url.spec().c_str(), /*allow_subpages=*/false},
  });

  ASSERT_TRUE(OpenPopup(settings_url));

  auto& session = GetKioskSystemSession();
  Browser& settings = CHECK_DEREF(session.GetSettingsBrowserForTesting());
  ASSERT_EQ(NextCommittedUrl(settings), settings_url);
}

IN_PROC_BROWSER_TEST_P(KioskSettingsTest, CanNavigateToSettingsSubUrl) {
  const GURL settings_url(kSettingsUrl);
  const GURL settings_suburl(base::StrCat({kSettingsUrl, "/suburl"}));

  ScopedSettingsPages scoped_pages({
      {/*url=*/settings_url.spec().c_str(), /*allow_subpages=*/true},
  });

  ASSERT_TRUE(OpenPopup(settings_suburl));

  auto& session = GetKioskSystemSession();
  Browser& settings = CHECK_DEREF(session.GetSettingsBrowserForTesting());
  EXPECT_EQ(NextCommittedUrl(settings), settings_suburl);
}

IN_PROC_BROWSER_TEST_P(KioskSettingsTest, CannotNavigateToNonSettingsUrl) {
  const GURL settings_url(kSettingsUrl);
  const GURL other_url("https://not-settings.com/");

  ScopedSettingsPages scoped_pages({
      {/*url=*/settings_url.spec().c_str(), /*allow_subpages=*/false},
  });

  // A new window directly to a disallowed page gets closed.
  ASSERT_FALSE(OpenPopup(other_url));

  // Navigating away from settings in an existing settings window doesn't work.
  ASSERT_TRUE(OpenPopup(settings_url));

  auto& session = GetKioskSystemSession();
  Browser& settings = CHECK_DEREF(session.GetSettingsBrowserForTesting());
  ASSERT_EQ(NextCommittedUrl(settings), settings_url);

  const GURL committed_url = NavigateInBrowser(settings, other_url);
  EXPECT_EQ(committed_url, settings_url);
}

IN_PROC_BROWSER_TEST_P(KioskSettingsTest, CannotNavigateToDisallowedSubUrl) {
  const GURL settings_url(kSettingsUrl);
  const GURL settings_suburl(
      base::StrCat({kSettingsUrl, "/disallowed-suburl"}));

  ScopedSettingsPages scoped_pages({
      {/*url=*/settings_url.spec().c_str(), /*allow_subpages=*/false},
  });

  // A new window directly to a non settings URL gets closed.
  ASSERT_FALSE(OpenPopup(settings_suburl));

  // Navigating away from settings in an existing settings window doesn't work.
  ASSERT_TRUE(OpenPopup(settings_url));
  auto& session = GetKioskSystemSession();
  Browser& settings = CHECK_DEREF(session.GetSettingsBrowserForTesting());
  ASSERT_EQ(NextCommittedUrl(settings), settings_url);

  const GURL committed_url = NavigateInBrowser(settings, settings_suburl);
  EXPECT_EQ(committed_url, settings_url);
}

IN_PROC_BROWSER_TEST_P(KioskSettingsTest, DoesNotOpenTwoSettingsBrowsers) {
  const GURL settings_url_1("https://settings-one.com/");
  const GURL settings_url_2("https://settings-two.com/");

  ScopedSettingsPages scoped_pages({
      {/*url=*/settings_url_1.spec().c_str(), /*allow_subpages=*/false},
      {/*url=*/settings_url_2.spec().c_str(), /*allow_subpages=*/false},
  });

  ASSERT_TRUE(OpenPopup(settings_url_1));

  auto& session = GetKioskSystemSession();
  Browser& first_settings = CHECK_DEREF(session.GetSettingsBrowserForTesting());
  ASSERT_EQ(NextCommittedUrl(first_settings), settings_url_1);

  ASSERT_TRUE(OpenPopup(settings_url_2));

  Browser& second_settings =
      CHECK_DEREF(session.GetSettingsBrowserForTesting());
  ASSERT_EQ(NextCommittedUrl(second_settings), settings_url_2);

  EXPECT_EQ(&first_settings, &second_settings);
}

IN_PROC_BROWSER_TEST_P(KioskSettingsTest,
                       NavigatingToSettingsInAppCreatesNewBrowser) {
  const GURL settings_url(kSettingsUrl);

  ScopedSettingsPages scoped_pages({
      {/*url=*/settings_url.spec().c_str(), /*allow_subpages=*/false},
  });

  // Navigation in the current tab creates a new browser of app type, and closes
  // the non-app one.
  Browser& browser = NavigateInCurrentTab(settings_url);
  EXPECT_FALSE(DidKioskCloseNewWindow());
  EXPECT_FALSE(DidKioskCloseNewWindow());

  Browser* settings = GetKioskSystemSession().GetSettingsBrowserForTesting();
  ASSERT_NE(settings, nullptr);
  EXPECT_NE(&browser, settings);
}

IN_PROC_BROWSER_TEST_P(KioskSettingsTest,
                       SettingsBrowserIsNullWhenSettingsIsClosed) {
  const GURL settings_url(kSettingsUrl);

  ScopedSettingsPages scoped_pages({
      {/*url=*/settings_url.spec().c_str(), /*allow_subpages=*/false},
  });

  // Settings browser is initially null since there are no settings windows.
  auto& session = GetKioskSystemSession();
  ASSERT_EQ(session.GetSettingsBrowserForTesting(), nullptr);

  // Settings browser is no longer null once a settings window opens.
  ASSERT_TRUE(OpenPopup(settings_url));
  Browser* settings = session.GetSettingsBrowserForTesting();
  ASSERT_NE(settings, nullptr);

  // Settings browser becomes null when the settings window closes.
  CloseBrowserSynchronously(settings);
  ASSERT_EQ(session.GetSettingsBrowserForTesting(), nullptr);
}

// Covers crbug.com/245088137, the settings could not reopen after losing focus.
IN_PROC_BROWSER_TEST_P(KioskSettingsTest, CanRefocusSettings) {
  const auto& pages = KioskSettingsNavigationThrottle::DefaultSettingsPages();
  ASSERT_GT(pages.size(), 1UL);

  ASSERT_TRUE(OpenPopup(GURL(pages[0].url)));

  auto& session = GetKioskSystemSession();
  Browser& settings = CHECK_DEREF(session.GetSettingsBrowserForTesting());

  // The settings browser is focused.
  EXPECT_TRUE(settings.window()->IsActive());

  // Simulate a focus switch.
  settings.window()->Deactivate();
  EXPECT_FALSE(settings.window()->IsActive());

  // Verify focus can switch to any other settings page.
  for (size_t i = 1; i < pages.size(); i++) {
    const GURL other_settings_page(pages[i].url);

    // Open another settings browser and expect navigation in the old window.
    auto& web_contents = ActiveWebContents(settings);
    content::TestNavigationObserver settings_navigation_observer(&web_contents,
                                                                 1);
    ASSERT_TRUE(OpenPopup(other_settings_page));
    // Wait for navigation to finish.
    settings_navigation_observer.Wait();

    // The settings browser should not have changed.
    ASSERT_EQ(&settings, session.GetSettingsBrowserForTesting());
    EXPECT_EQ(web_contents.GetLastCommittedURL(), other_settings_page);

    // The settings browser should be focused again.
    EXPECT_TRUE(settings.window()->IsActive());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskSettingsTest,
    testing::ValuesIn(KioskMixin::ConfigsToAutoLaunchEachAppType()),
    KioskMixin::ConfigName);

}  // namespace ash

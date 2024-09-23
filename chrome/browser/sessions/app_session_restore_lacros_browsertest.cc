// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

class AppSessionRestoreTest : public InProcessBrowserTest {
 public:
  AppSessionRestoreTest() {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{// TODO(crbug.com/40248833): Use HTTPS URLs in
                               // tests to avoid having to
                               // disable this feature.
                               features::kHttpsUpgrades});
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  }
  ~AppSessionRestoreTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    if (browser()) {
      SessionStartupPref pref(SessionStartupPref::LAST);
      SessionStartupPref::SetStartupPref(browser()->profile(), pref);
    }
  }

  webapps::AppId InstallPWA(Profile* profile, const GURL& start_url) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
    web_app_info->title = u"A Web App";
    webapps::AppId app_id =
        web_app::test::InstallWebApp(profile, std::move(web_app_info));
    apps::AppReadinessWaiter(profile, app_id).Await();
    return app_id;
  }

 private:
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  base::test::ScopedFeatureList scoped_feature_list_;
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
};

// This test ensures AppSessionService is notified of app restorations
// correctly.
IN_PROC_BROWSER_TEST_F(AppSessionRestoreTest, CtrlShiftTRestoresAppsCorrectly) {
  Profile* profile = browser()->profile();
  auto example_url = GURL("http://www.example.com");
  auto example_url2 = GURL("http://www.example2.com");
  auto example_url3 = GURL("http://www.example3.com");

  // Install 3 PWAs.
  webapps::AppId app_id = InstallPWA(profile, example_url);
  webapps::AppId app_id2 = InstallPWA(profile, example_url2);
  webapps::AppId app_id3 = InstallPWA(profile, example_url3);

  // Open all 3, browser 2 is app_popup.
  Browser* app_browser = web_app::LaunchWebAppBrowserAndWait(profile, app_id);
  Browser* app_browser2 = web_app::LaunchWebAppBrowserAndWait(
      profile, app_id2, WindowOpenDisposition::NEW_POPUP);
  Browser* app_browser3 = web_app::LaunchWebAppBrowserAndWait(profile, app_id3);

  // 3 apps + basic browser.
  ASSERT_EQ(4u, BrowserList::GetInstance()->size());

  // Close all 3.
  CloseBrowserSynchronously(app_browser);
  CloseBrowserSynchronously(app_browser2);
  CloseBrowserSynchronously(app_browser3);

  // Just the basic browser.
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());

  // Ctrl-Shift-T 3 times.
  chrome::RestoreTab(browser());
  chrome::RestoreTab(browser());
  chrome::RestoreTab(browser());

  // Ensure there's 4. Three apps, plus 1 basic test browser.
  bool app1_seen = false;
  bool app2_seen = false;
  bool app3_seen = false;
  ASSERT_EQ(4u, BrowserList::GetInstance()->size());
  for (Browser* browser : *(BrowserList::GetInstance())) {
    if (web_app::AppBrowserController::IsForWebApp(browser, app_id)) {
      EXPECT_FALSE(app1_seen);
      EXPECT_TRUE(browser->is_type_app());
      app1_seen = true;
    } else if (web_app::AppBrowserController::IsForWebApp(browser, app_id2)) {
      EXPECT_FALSE(app2_seen);
      EXPECT_TRUE(browser->is_type_app_popup());
      app2_seen = true;
    } else if (web_app::AppBrowserController::IsForWebApp(browser, app_id3)) {
      EXPECT_FALSE(app3_seen);
      EXPECT_TRUE(browser->is_type_app());
      app3_seen = true;
    }
  }
  EXPECT_TRUE(app1_seen);
  EXPECT_TRUE(app2_seen);
  EXPECT_TRUE(app3_seen);
}

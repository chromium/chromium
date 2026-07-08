// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ssl_browsertest_base.h"
#include "chrome/browser/ssl/ssl_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/base/features.h"

class SSLUITestWithWebApps : public InteractiveBrowserTestMixin<SSLUITestBase> {
 public:
  SSLUITestWithWebApps() {
    auto disabled_features = SSLUITestBase::GetDisabledFeatures();
#if BUILDFLAG(IS_CHROMEOS)
    // TODO(crbug.com/532595481): Disabling navigation capturing as a
    // workaround for flakiness on ChromeOS due to reentrant behavior in
    // WebAppPublisherHelper.
    disabled_features.push_back(features::kPwaNavigationCapturing);
#endif  // BUILDFLAG(IS_CHROMEOS)
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{net::features::kVerifyQWACs}, disabled_features);
  }

  Browser* InstallAndOpenTestWebApp(const GURL& start_url) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->title = u"Test app";
    web_app_info->description = u"Test description";

    Profile* profile = browser()->profile();

    webapps::AppId app_id =
        web_app::test::InstallWebApp(profile, std::move(web_app_info));

    Browser* app_browser = web_app::LaunchWebAppBrowserAndWait(profile, app_id);
    ui_test_utils::WaitUntilBrowserBecomeActive(app_browser);
    return app_browser;
  }

  // Helper function that checks that after proceeding through an interstitial,
  // the app window is closed, a new tab with the app URL is opened, and there
  // is no interstitial.
  void ProceedThroughInterstitialInAppAndCheckNewTabOpened(
      Browser* app_browser,
      const GURL& app_url) {
    Profile* profile = browser()->profile();

    size_t num_browsers =
        ProfileBrowserCollection::GetForProfile(profile)->GetSize();
    EXPECT_TRUE(ui_test_utils::IsBrowserActive(app_browser));
    int num_tabs = browser()->tab_strip_model()->count();

    ProceedThroughInterstitial(
        app_browser->tab_strip_model()->GetActiveWebContents());
    ui_test_utils::WaitUntilBrowserBecomeActive(browser());

    EXPECT_EQ(--num_browsers,
              ProfileBrowserCollection::GetForProfile(profile)->GetSize());
    EXPECT_TRUE(ui_test_utils::IsBrowserActive(browser()));
    EXPECT_EQ(++num_tabs, browser()->tab_strip_model()->count());

    content::WebContents* new_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(new_tab));

    ssl_test_util::CheckAuthenticationBrokenState(
        new_tab, net::CERT_STATUS_DATE_INVALID, ssl_test_util::AuthState::NONE);
    EXPECT_EQ(app_url, new_tab->GetVisibleURL());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

// Visits a page in an app window with https error and proceed.
IN_PROC_BROWSER_TEST_F(SSLUITestWithWebApps,
                       InAppTestHTTPSExpiredCertAndProceed) {
  ASSERT_TRUE(https_server_expired_.Start());

  const GURL app_url = https_server_expired_.GetURL("/ssl/google.html");
  Browser* app_browser = InstallAndOpenTestWebApp(app_url);

  content::WebContents* app_tab =
      app_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(app_tab));
  ssl_test_util::CheckAuthenticationBrokenState(
      app_tab, net::CERT_STATUS_DATE_INVALID,
      ssl_test_util::AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitialInAppAndCheckNewTabOpened(app_browser, app_url);
}

// Visits a page with https error and proceed. Then open the app and proceed.
IN_PROC_BROWSER_TEST_F(SSLUITestWithWebApps,
                       InAppTestHTTPSExpiredCertAndPreviouslyProceeded) {
  ASSERT_TRUE(https_server_expired_.Start());

  const GURL app_url = https_server_expired_.GetURL("/ssl/google.html");

  // Go through the interstitial in a regular browser tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(initial_tab));
  ssl_test_util::CheckAuthenticationBrokenState(
      initial_tab, net::CERT_STATUS_DATE_INVALID,
      ssl_test_util::AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(initial_tab);
  ssl_test_util::CheckAuthenticationBrokenState(initial_tab,
                                                net::CERT_STATUS_DATE_INVALID,
                                                ssl_test_util::AuthState::NONE);

  Browser* app_browser = InstallAndOpenTestWebApp(app_url);

  // Apps are not allowed to have SSL errors, so the interstitial should be
  // showing even though the user proceeded through it in a regular tab.
  content::WebContents* app_tab =
      app_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(app_tab));

  ProceedThroughInterstitialInAppAndCheckNewTabOpened(app_browser, app_url);
}

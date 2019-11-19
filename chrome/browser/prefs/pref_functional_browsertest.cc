// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::BrowserContext;
using content::DownloadManager;

class PrefsFunctionalTest : public InProcessBrowserTest {
 protected:
  // Create a DownloadTestObserverTerminal that will wait for the
  // specified number of downloads to finish.
  std::unique_ptr<content::DownloadTestObserver> CreateWaiter(
      Browser* browser,
      int num_downloads) {
    DownloadManager* download_manager =
        BrowserContext::GetDownloadManager(browser->profile());

    content::DownloadTestObserver* downloads_observer =
         new content::DownloadTestObserverTerminal(
             download_manager,
             num_downloads,
             content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
    return base::WrapUnique(downloads_observer);
  }
};

IN_PROC_BROWSER_TEST_F(PrefsFunctionalTest, TestDownloadDirPref) {
  ASSERT_TRUE(embedded_test_server()->Start());

  base::FilePath new_download_dir =
      DownloadPrefs(browser()->profile()).DownloadPath().AppendASCII("subdir");
  base::FilePath downloaded_pkg =
      new_download_dir.AppendASCII("a_zip_file.zip");

  // Set pref to download in new_download_dir.
  browser()->profile()->GetPrefs()->SetFilePath(
      prefs::kDownloadDefaultDirectory, new_download_dir);

  // Create a downloads observer.
  std::unique_ptr<content::DownloadTestObserver> downloads_observer(
      CreateWaiter(browser(), 1));
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/downloads/a_zip_file.zip"));
  // Waits for the download to complete.
  downloads_observer->WaitForFinished();

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(downloaded_pkg));
}

// Verify image content settings show or hide images.
IN_PROC_BROWSER_TEST_F(PrefsFunctionalTest, TestImageContentSettings) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/settings/image_page.html"));

  bool result = false;
  std::string script =
      "for (i=0; i < document.images.length; i++) {"
      "  if ((document.images[i].naturalWidth != 0) &&"
      "      (document.images[i].naturalHeight != 0)) {"
      "    window.domAutomationController.send(true);"
      "  }"
      "}"
      "window.domAutomationController.send(false);";
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      script,
      &result));
  EXPECT_TRUE(result);

  browser()->profile()->GetPrefs()->SetInteger(
      content_settings::WebsiteSettingsRegistry::GetInstance()
          ->Get(ContentSettingsType::IMAGES)
          ->default_value_pref_name(),
      CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/settings/image_page.html"));

  result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      script,
      &result));
  EXPECT_FALSE(result);
}

// Verify that enabling/disabling Javascript in prefs works.
IN_PROC_BROWSER_TEST_F(PrefsFunctionalTest, TestJavascriptEnableDisable) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kWebKitJavascriptEnabled));
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/javaScriptTitle.html"));
  EXPECT_EQ(base::ASCIIToUTF16("Title from script javascript enabled"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kWebKitJavascriptEnabled,
                                               false);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/javaScriptTitle.html"));
  EXPECT_EQ(base::ASCIIToUTF16("This is html title"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
}

// Verify restore for bookmark bar visibility.
IN_PROC_BROWSER_TEST_F(PrefsFunctionalTest,
                       TestSessionRestoreShowBookmarkBar) {
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      bookmarks::prefs::kShowBookmarkBar));
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowBookmarkBar, true);
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      bookmarks::prefs::kShowBookmarkBar));

  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      bookmarks::prefs::kShowBookmarkBar));
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());
}

// Verify images are not blocked in incognito mode.
IN_PROC_BROWSER_TEST_F(PrefsFunctionalTest, TestImagesNotBlockedInIncognito) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/settings/image_page.html");
  Browser* incognito_browser = CreateIncognitoBrowser();
  ui_test_utils::NavigateToURL(incognito_browser, url);

  bool result = false;
  std::string script =
      "for (i=0; i < document.images.length; i++) {"
      "  if ((document.images[i].naturalWidth != 0) &&"
      "      (document.images[i].naturalHeight != 0)) {"
      "    window.domAutomationController.send(true);"
      "  }"
      "}"
      "window.domAutomationController.send(false);";
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      incognito_browser->tab_strip_model()->GetActiveWebContents(),
      script,
      &result));
  EXPECT_TRUE(result);
}

// Verify setting homepage preference to newtabpage across restarts. Part1
IN_PROC_BROWSER_TEST_F(PrefsFunctionalTest, PRE_TestHomepageNewTabpagePrefs) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kHomePageIsNewTabPage,
                                               true);
}

// Verify setting homepage preference to newtabpage across restarts. Part2
IN_PROC_BROWSER_TEST_F(PrefsFunctionalTest, TestHomepageNewTabpagePrefs) {
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kHomePageIsNewTabPage));
}

// Verify setting homepage preference to specific url. Part1
IN_PROC_BROWSER_TEST_F(PrefsFunctionalTest, PRE_TestHomepagePrefs) {
  GURL home_page_url("http://www.google.com");

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);
  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kHomePage);
  if (pref && !pref->IsManaged()) {
    prefs->SetString(prefs::kHomePage, home_page_url.spec());
  }
}

// Verify setting homepage preference to specific url. Part2
IN_PROC_BROWSER_TEST_F(PrefsFunctionalTest, TestHomepagePrefs) {
  GURL home_page_url("http://www.google.com");

  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
  EXPECT_EQ(home_page_url.spec(), prefs->GetString(prefs::kHomePage));
}

// Verify the security preference under privacy across restarts. Part1
IN_PROC_BROWSER_TEST_F(PrefsFunctionalTest, PRE_TestPrivacySecurityPrefs) {
  PrefService* prefs = browser()->profile()->GetPrefs();

  static_assert(chrome_browser_net::NETWORK_PREDICTION_DEFAULT !=
                    chrome_browser_net::NETWORK_PREDICTION_NEVER,
                "PrefsFunctionalTest.TestPrivacySecurityPrefs relies on "
                "predictive network actions being enabled by default.");
  EXPECT_EQ(chrome_browser_net::NETWORK_PREDICTION_DEFAULT,
            prefs->GetInteger(prefs::kNetworkPredictionOptions));
  prefs->SetInteger(prefs::kNetworkPredictionOptions,
                    chrome_browser_net::NETWORK_PREDICTION_NEVER);

  EXPECT_TRUE(prefs->GetBoolean(prefs::kSafeBrowsingEnabled));
  prefs->SetBoolean(prefs::kSafeBrowsingEnabled, false);

  EXPECT_TRUE(prefs->GetBoolean(prefs::kAlternateErrorPagesEnabled));
  prefs->SetBoolean(prefs::kAlternateErrorPagesEnabled, false);

  EXPECT_TRUE(prefs->GetBoolean(prefs::kSearchSuggestEnabled));
  prefs->SetBoolean(prefs::kSearchSuggestEnabled, false);
}

// Verify the security preference under privacy across restarts. Part2
IN_PROC_BROWSER_TEST_F(PrefsFunctionalTest, TestPrivacySecurityPrefs) {
  PrefService* prefs = browser()->profile()->GetPrefs();

  EXPECT_EQ(chrome_browser_net::NETWORK_PREDICTION_NEVER,
            prefs->GetInteger(prefs::kNetworkPredictionOptions));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kSafeBrowsingEnabled));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kAlternateErrorPagesEnabled));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kSearchSuggestEnabled));
}

// Verify that we have some Local State prefs.
IN_PROC_BROWSER_TEST_F(PrefsFunctionalTest, TestHaveLocalStatePrefs) {
  EXPECT_TRUE(g_browser_process->local_state()
                  ->GetPreferenceValues(PrefService::INCLUDE_DEFAULTS)
                  .get());
}

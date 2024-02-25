// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/l10n/l10n_util.h"

class SettingsAppIntegrationTest : public ash::SystemWebAppIntegrationTest {};

// Test that the Settings App installs and launches correctly.
IN_PROC_BROWSER_TEST_P(SettingsAppIntegrationTest, SettingsApp) {
  const GURL url("chrome://os-settings");
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      ash::SystemWebAppType::SETTINGS, url, "Settings"));
}

// Test that the Settings App installs correctly when it's set to be disabled
// via SystemFeaturesDisableList policy, but doesn't launch.
IN_PROC_BROWSER_TEST_P(SettingsAppIntegrationTest, SettingsAppDisabled) {
  {
    ScopedListPrefUpdate update(
        TestingBrowserProcess::GetGlobal()->local_state(),
        policy::policy_prefs::kSystemFeaturesDisableList);
    update->Append(static_cast<int>(policy::SystemFeature::kOsSettings));
  }

  ASSERT_FALSE(GetManager()
                   .GetAppIdForSystemApp(ash::SystemWebAppType::SETTINGS)
                   .has_value());

  WaitForTestSystemAppInstall();

  // Don't wait for load here, because we navigate to chrome error page instead.
  // The App's launch URL won't be loaded.
  Browser* app_browser;
  LaunchAppWithoutWaiting(ash::SystemWebAppType::SETTINGS, &app_browser);

  ASSERT_TRUE(GetManager()
                  .GetAppIdForSystemApp(ash::SystemWebAppType::SETTINGS)
                  .has_value());

  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  content::WebUI* web_ui = web_contents->GetWebUI();
  ASSERT_TRUE(web_ui);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_HEADER),
            web_contents->GetTitle());
}

// This test verifies that the settings page is opened in a new browser window.
IN_PROC_BROWSER_TEST_P(SettingsAppIntegrationTest, OmniboxNavigateToSettings) {
  // Install the Settings App.
  ash::SystemWebAppManager::GetForTest(browser()->profile())
      ->InstallSystemAppsForTesting();
  GURL old_url = browser()->tab_strip_model()->GetActiveWebContents()->GetURL();
  {
    ui_test_utils::AllBrowserTabAddedWaiter waiter;
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
        browser()->profile());
    auto* web_contents = waiter.Wait();
    ASSERT_TRUE(web_contents);
    content::WaitForLoadStop(web_contents);
  }
  // browser() tab contents should be unaffected.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(old_url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Settings page should be opened in a new window.
  Browser* settings_browser =
      chrome::SettingsWindowManager::GetInstance()->FindBrowserForProfile(
          browser()->profile());
  EXPECT_NE(browser(), settings_browser);
  EXPECT_EQ(
      GURL(chrome::GetOSSettingsUrl(std::string())),
      settings_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_P(SettingsAppIntegrationTest,
                       RedirectIncognitoToOriginalProfile) {
  // Install the real SWA, not the test mock. This verifies the production
  // SystemAppInfo is correct.
  ash::SystemWebAppManager::GetForTest(browser()->profile())
      ->InstallSystemAppsForTesting();

  // When launching from incognito profile, OS Settings gets launched to the
  // original profile.
  Profile* incognito_profile =
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ui_test_utils::BrowserChangeObserver browser_opened(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  ash::LaunchSystemWebAppAsync(incognito_profile,
                               ash::SystemWebAppType::SETTINGS);
  browser_opened.Wait();

  // There should be a browser for the original profile, but not the incognito
  // profile.
  auto* manager = chrome::SettingsWindowManager::GetInstance();
  EXPECT_TRUE(manager->FindBrowserForProfile(browser()->profile()));
  EXPECT_FALSE(manager->FindBrowserForProfile(incognito_profile));
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SettingsAppIntegrationTest);

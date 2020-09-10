// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/chromeos/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/l10n/l10n_util.h"

class SettingsAppIntegrationTest : public SystemWebAppIntegrationTest {};

// Test that the Settings App installs and launches correctly.
IN_PROC_BROWSER_TEST_P(SettingsAppIntegrationTest, SettingsApp) {
  const GURL url("chrome://os-settings");
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      web_app::SystemAppType::SETTINGS, url, "Settings"));
}

// Test that the Settings App installs correctly when it's set to be disabled
// via SystemFeaturesDisableList policy, but doesn't launch.
IN_PROC_BROWSER_TEST_P(SettingsAppIntegrationTest, SettingsAppDisabled) {
  {
    ListPrefUpdate update(TestingBrowserProcess::GetGlobal()->local_state(),
                          policy::policy_prefs::kSystemFeaturesDisableList);
    base::ListValue* list = update.Get();
    list->Append(policy::SystemFeature::OS_SETTINGS);
  }

  ASSERT_FALSE(GetManager()
                   .GetAppIdForSystemApp(web_app::SystemAppType::SETTINGS)
                   .has_value());

  WaitForTestSystemAppInstall();

  // Don't wait for load here, because we navigate to chrome error page instead.
  // The App's launch URL won't be loaded.
  Browser* app_browser;
  LaunchAppWithoutWaiting(web_app::SystemAppType::SETTINGS, &app_browser);

  ASSERT_TRUE(GetManager()
                  .GetAppIdForSystemApp(web_app::SystemAppType::SETTINGS)
                  .has_value());

  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  content::WebUI* web_ui = web_contents->GetCommittedWebUI();
  ASSERT_TRUE(web_ui);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_HEADER),
            web_contents->GetTitle());
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_MANIFEST_INSTALL_P(
    SettingsAppIntegrationTest);

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/content/scanning/url_constants.h"
#include "base/values.h"
#include "chrome/browser/ash/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/chromeos/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using ScanningAppIntegrationTest = SystemWebAppIntegrationTest;

// Test that the Scanning App installs and launches correctly by running some
// spot checks on the manifest.
IN_PROC_BROWSER_TEST_P(ScanningAppIntegrationTest, ScanningAppInLauncher) {
  const GURL url(ash::kChromeUIScanningAppUrl);
  EXPECT_NO_FATAL_FAILURE(
      ExpectSystemWebAppValid(web_app::SystemAppType::SCANNING, url, "Scan"));
}

// Test that the Scanning App installs correctly but doesn't launch when it's
// set to be disabled via the SystemFeaturesDisableList policy.
IN_PROC_BROWSER_TEST_P(ScanningAppIntegrationTest, ScanningAppDisabled) {
  {
    ListPrefUpdate update(TestingBrowserProcess::GetGlobal()->local_state(),
                          policy::policy_prefs::kSystemFeaturesDisableList);
    base::ListValue* list = update.Get();
    list->Append(policy::SystemFeature::kScanning);
  }

  ASSERT_FALSE(GetManager()
                   .GetAppIdForSystemApp(web_app::SystemAppType::SCANNING)
                   .has_value());

  WaitForTestSystemAppInstall();

  // Launch the app without waiting since the Chrome error page will be loaded
  // instead of the app's URL.
  Browser* app_browser;
  LaunchAppWithoutWaiting(web_app::SystemAppType::SCANNING, &app_browser);

  ASSERT_TRUE(GetManager()
                  .GetAppIdForSystemApp(web_app::SystemAppType::SCANNING)
                  .has_value());

  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  content::WebUI* web_ui = web_contents->GetCommittedWebUI();
  ASSERT_TRUE(web_ui);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_HEADER),
            web_contents->GetTitle());
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    ScanningAppIntegrationTest);

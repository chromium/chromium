// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/web_preferences.h"

class ChromeContentBrowserClientChromeOsPartTest : public InProcessBrowserTest {
 public:
  ChromeContentBrowserClientChromeOsPartTest() {
    feature_list_.InitAndEnableFeature(chromeos::features::kSplitSettings);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientChromeOsPartTest,
                       SettingsWindowFontSize) {
  // Install the Settings App.
  web_app::WebAppProvider::Get(browser()->profile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  const content::WebPreferences kDefaultPrefs;
  const int kDefaultFontSize = kDefaultPrefs.default_font_size;
  const int kDefaultFixedFontSize = kDefaultPrefs.default_fixed_font_size;

  // Set the browser font sizes to non-default values.
  Profile* profile = browser()->profile();
  PrefService* profile_prefs = profile->GetPrefs();
  profile_prefs->SetInteger(prefs::kWebKitDefaultFontSize,
                            kDefaultFontSize + 2);
  profile_prefs->SetInteger(prefs::kWebKitDefaultFixedFontSize,
                            kDefaultFixedFontSize + 1);

  // Open the OS settings window.
  auto* settings = chrome::SettingsWindowManager::GetInstance();
  settings->ShowOSSettings(profile);

  // The OS settings window still uses the default font sizes.
  Browser* browser = settings->FindBrowserForProfile(profile);
  auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();
  content::WebPreferences window_prefs =
      web_contents->GetRenderViewHost()->GetWebkitPreferences();
  EXPECT_EQ(kDefaultFontSize, window_prefs.default_font_size);
  EXPECT_EQ(kDefaultFixedFontSize, window_prefs.default_fixed_font_size);
}

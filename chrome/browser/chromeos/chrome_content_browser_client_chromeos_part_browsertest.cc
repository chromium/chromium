// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

using ChromeContentBrowserClientChromeOsPartTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientChromeOsPartTest,
                       SettingsWindowFontSize) {
  // Install the Settings App.
  web_app::WebAppProvider::Get(browser()->profile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  const blink::web_pref::WebPreferences kDefaultPrefs;
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

  // The above ShowOSSettings() should trigger an asynchronous call to launch
  // OS Settings SWA. Flush Mojo calls so the browser window is created.
  web_app::FlushSystemWebAppLaunchesForTesting(browser()->profile());

  // The OS settings window still uses the default font sizes.
  Browser* browser = settings->FindBrowserForProfile(profile);
  auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();
  blink::web_pref::WebPreferences window_prefs =
      web_contents->GetOrCreateWebPreferences();
  EXPECT_EQ(kDefaultFontSize, window_prefs.default_font_size);
  EXPECT_EQ(kDefaultFixedFontSize, window_prefs.default_fixed_font_size);
}

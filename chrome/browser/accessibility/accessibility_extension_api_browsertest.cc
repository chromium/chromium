// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/base/ui_base_features.h"

namespace extensions {

using AccessibilityPrivateApiTest = ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(AccessibilityPrivateApiTest, SendSyntheticKeyEvent) {
  ASSERT_TRUE(RunExtensionSubtest("accessibility_private/",
                                  "send_synthetic_key_event.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AccessibilityPrivateApiTest, GetDisplayLanguageTest) {
  ASSERT_TRUE(
      RunExtensionSubtest("accessibility_private/", "display_language.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AccessibilityPrivateApiTest, OpenSettingsSubpage) {
  Profile* profile = chromeos::AccessibilityManager::Get()->profile();

  // Install the Settings App.
  web_app::WebAppProvider::Get(profile)
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  ASSERT_TRUE(RunExtensionSubtest("accessibility_private/",
                                  "open_settings_subpage.html"))
      << message_;

  chrome::SettingsWindowManager* settings_manager =
      chrome::SettingsWindowManager::GetInstance();

  Browser* settings_browser = settings_manager->FindBrowserForProfile(profile);
  EXPECT_NE(nullptr, settings_browser);

  content::WebContents* web_contents =
      settings_browser->tab_strip_model()->GetWebContentsAt(0);

  WaitForLoadStop(web_contents);

  EXPECT_EQ(GURL(chrome::GetOSSettingsUrl("manageAccessibility/tts")),
            web_contents->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(AccessibilityPrivateApiTest,
                       OpenSettingsSubpage_InvalidSubpage) {
  Profile* profile = chromeos::AccessibilityManager::Get()->profile();

  // Install the Settings App.
  web_app::WebAppProvider::Get(profile)
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  ASSERT_TRUE(RunExtensionSubtest("accessibility_private/",
                                  "open_settings_subpage_invalid_subpage.html"))
      << message_;

  chrome::SettingsWindowManager* settings_manager =
      chrome::SettingsWindowManager::GetInstance();

  // Invalid subpage should not open settings window.
  Browser* settings_browser = settings_manager->FindBrowserForProfile(profile);
  EXPECT_EQ(nullptr, settings_browser);
}

}  // namespace extensions

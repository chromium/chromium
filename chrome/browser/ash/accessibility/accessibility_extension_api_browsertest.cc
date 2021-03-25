// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/ui/accessibility_confirmation_dialog.h"
#include "ash/shell.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/result_catcher.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ui_base_features.h"

namespace extensions {

using ::ash::AccessibilityManager;

class AccessibilityPrivateApiTest : public ExtensionApiTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(
        ::features::kSelectToSpeakNavigationControl);
    ExtensionApiTest::SetUp();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AccessibilityPrivateApiTest, SendSyntheticKeyEvent) {
  ASSERT_TRUE(RunExtensionSubtest("accessibility_private/",
                                  "send_synthetic_key_event.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AccessibilityPrivateApiTest,
                       GetDisplayNameForLocaleTest) {
  ASSERT_TRUE(
      RunExtensionSubtest("accessibility_private/", "display_locale.html"))
      << message_;
}

// Flaky on a debug build, see crbug.com/1030507.
#if !defined(NDEBUG)
#define MAYBE_OpenSettingsSubpage DISABLED_OpenSettingsSubpage
#else
#define MAYBE_OpenSettingsSubpage OpenSettingsSubpage
#endif
IN_PROC_BROWSER_TEST_F(AccessibilityPrivateApiTest, MAYBE_OpenSettingsSubpage) {
  Profile* profile = AccessibilityManager::Get()->profile();

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

  EXPECT_TRUE(WaitForLoadStop(web_contents));

  EXPECT_EQ(GURL(chrome::GetOSSettingsUrl("manageAccessibility/tts")),
            web_contents->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(AccessibilityPrivateApiTest,
                       OpenSettingsSubpage_InvalidSubpage) {
  Profile* profile = AccessibilityManager::Get()->profile();

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

IN_PROC_BROWSER_TEST_F(AccessibilityPrivateApiTest,
                       IsFeatureEnabled_FeatureDisabled) {
  ASSERT_TRUE(RunExtensionSubtest("accessibility_private/",
                                  "is_feature_enabled_feature_disabled.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AccessibilityPrivateApiTest, AcceptConfirmationDialog) {
  ASSERT_TRUE(RunExtensionSubtest("accessibility_private/",
                                  "accept_confirmation_dialog.html"))
      << message_;

  // The test has requested to open the confirmation dialog. Check that
  // it was created, then confirm it.
  ash::AccessibilityConfirmationDialog* dialog_ =
      ash::Shell::Get()
          ->accessibility_controller()
          ->GetConfirmationDialogForTest();
  ASSERT_NE(dialog_, nullptr);

  EXPECT_EQ(dialog_->GetWindowTitle(), u"Confirm me! 🐶");

  // Accept the dialog and wait for the JS test to get the confirmation.
  ResultCatcher catcher;
  dialog_->Accept();
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(AccessibilityPrivateApiTest, CancelConfirmationDialog) {
  ASSERT_TRUE(RunExtensionSubtest("accessibility_private/",
                                  "cancel_confirmation_dialog.html"))
      << message_;

  // The test has requested to open the confirmation dialog. Check that
  // it was created, then cancel it.
  ash::AccessibilityConfirmationDialog* dialog_ =
      ash::Shell::Get()
          ->accessibility_controller()
          ->GetConfirmationDialogForTest();
  ASSERT_NE(dialog_, nullptr);

  EXPECT_EQ(dialog_->GetWindowTitle(), u"Cancel me!");

  // Cancel the dialog and wait for the JS test to get the callback.
  ResultCatcher catcher;
  dialog_->Cancel();
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(AccessibilityPrivateApiTest, CloseConfirmationDialog) {
  ASSERT_TRUE(RunExtensionSubtest("accessibility_private/",
                                  "cancel_confirmation_dialog.html"))
      << message_;

  // The test has requested to open the confirmation dialog. Check that
  // it was created, then close it.
  ash::AccessibilityConfirmationDialog* dialog_ =
      ash::Shell::Get()
          ->accessibility_controller()
          ->GetConfirmationDialogForTest();
  ASSERT_TRUE(dialog_ != nullptr);

  EXPECT_EQ(dialog_->GetWindowTitle(), u"Cancel me!");

  // Close the dialog and wait for the JS test to get the callback.
  ResultCatcher catcher;
  dialog_->Close();
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

class AccessibilityPrivateApiFeatureEnabledTest : public ExtensionApiTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kSelectToSpeakNavigationControl);
    ExtensionApiTest::SetUp();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AccessibilityPrivateApiFeatureEnabledTest,
                       IsFeatureEnabled_FeatureEnabled) {
  ASSERT_TRUE(RunExtensionSubtest("accessibility_private/",
                                  "is_feature_enabled_feature_enabled.html"))
      << message_;
}

}  // namespace extensions

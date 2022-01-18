// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/ui/accessibility_confirmation_dialog.h"
#include "ash/shell.h"
#include "ash/system/accessibility/dictation_bubble_controller.h"
#include "ash/system/accessibility/dictation_bubble_view.h"
#include "base/test/scoped_feature_list.h"
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
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ui_base_features.h"

namespace ash {

using ContextType = ::extensions::ExtensionBrowserTest::ContextType;

class AccessibilityPrivateApiTest
    : public extensions::ExtensionApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  AccessibilityPrivateApiTest() : ExtensionApiTest(GetParam()) {}
  ~AccessibilityPrivateApiTest() override = default;
  AccessibilityPrivateApiTest& operator=(const AccessibilityPrivateApiTest&) =
      delete;
  AccessibilityPrivateApiTest(const AccessibilityPrivateApiTest&) = delete;

 protected:
  // ExtensionApiTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kExperimentalAccessibilityDictationCommands);
  }

  [[nodiscard]] bool RunSubtest(const char* subtest) {
    return RunExtensionTest("accessibility_private", {.custom_arg = subtest});
  }

  bool IsDictationBubbleVisible() {
    DictationBubbleController* controller =
        Shell::Get()
            ->accessibility_controller()
            ->GetDictationBubbleControllerForTest();
    DCHECK(controller != nullptr);
    return controller->widget_->IsVisible();
  }

  std::u16string GetDictationBubbleText() {
    DictationBubbleController* controller =
        Shell::Get()
            ->accessibility_controller()
            ->GetDictationBubbleControllerForTest();
    DCHECK(controller != nullptr);
    return controller->dictation_bubble_view_->GetTextForTesting();
  }

  bool IsDictationBubbleStandbyImageVisible() {
    DictationBubbleController* controller =
        Shell::Get()
            ->accessibility_controller()
            ->GetDictationBubbleControllerForTest();
    DCHECK(controller != nullptr);
    return controller->dictation_bubble_view_
        ->IsStandbyImageVisibleForTesting();
  }

  bool IsDictationBubbleMacroSucceededImageVisible() {
    DictationBubbleController* controller =
        Shell::Get()
            ->accessibility_controller()
            ->GetDictationBubbleControllerForTest();
    DCHECK(controller != nullptr);
    return controller->dictation_bubble_view_
        ->IsMacroSucceededImageVisibleForTesting();
  }

  bool IsDictationBubbleMacroFailedImageVisible() {
    DictationBubbleController* controller =
        Shell::Get()
            ->accessibility_controller()
            ->GetDictationBubbleControllerForTest();
    DCHECK(controller != nullptr);
    return controller->dictation_bubble_view_
        ->IsMacroFailedImageVisibleForTesting();
  }

  DictationBubbleIconType GetDictationBubbleVisibleIcon() {
    DCHECK_GE(1, IsDictationBubbleStandbyImageVisible() +
                     IsDictationBubbleMacroSucceededImageVisible() +
                     IsDictationBubbleMacroFailedImageVisible())
        << "No more than one icon should be visible!";
    if (IsDictationBubbleStandbyImageVisible())
      return DictationBubbleIconType::kStandby;
    if (IsDictationBubbleMacroSucceededImageVisible())
      return DictationBubbleIconType::kMacroSuccess;
    if (IsDictationBubbleMacroFailedImageVisible())
      return DictationBubbleIconType::kMacroFail;
    return DictationBubbleIconType::kHidden;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiTest, SendSyntheticKeyEvent) {
  ASSERT_TRUE(RunSubtest("testSendSyntheticKeyEvent")) << message_;
}

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiTest,
                       GetDisplayNameForLocaleTest) {
  ASSERT_TRUE(RunSubtest("testGetDisplayNameForLocale")) << message_;
}

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiTest, OpenSettingsSubpage) {
  Profile* profile = AccessibilityManager::Get()->profile();

  // Install the Settings App.
  web_app::WebAppProvider::GetForTest(profile)
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  ASSERT_TRUE(RunSubtest("testOpenSettingsSubpage")) << message_;

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

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiTest,
                       OpenSettingsSubpage_InvalidSubpage) {
  Profile* profile = AccessibilityManager::Get()->profile();

  // Install the Settings App.
  web_app::WebAppProvider::GetForTest(profile)
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  ASSERT_TRUE(RunSubtest("testOpenSettingsSubpageInvalidSubpage")) << message_;

  chrome::SettingsWindowManager* settings_manager =
      chrome::SettingsWindowManager::GetInstance();

  // Invalid subpage should not open settings window.
  Browser* settings_browser = settings_manager->FindBrowserForProfile(profile);
  EXPECT_EQ(nullptr, settings_browser);
}

template <bool enabled>
class AccessibilityPrivateApiFeatureTest : public AccessibilityPrivateApiTest {
 public:
  AccessibilityPrivateApiFeatureTest() = default;
  ~AccessibilityPrivateApiFeatureTest() override = default;
  AccessibilityPrivateApiFeatureTest& operator=(
      const AccessibilityPrivateApiFeatureTest&) = delete;
  AccessibilityPrivateApiFeatureTest(
      const AccessibilityPrivateApiFeatureTest&) = delete;

  // AccessibilityPrivateApiTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    AccessibilityPrivateApiTest::SetUpCommandLine(command_line);
    if (enabled) {
      scoped_feature_list_.InitAndEnableFeature(
          ::features::kEnhancedNetworkVoices);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          ::features::kEnhancedNetworkVoices);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

using AccessibilityPrivateApiFeatureDisabledTest =
    AccessibilityPrivateApiFeatureTest<false>;
using AccessibilityPrivateApiFeatureEnabledTest =
    AccessibilityPrivateApiFeatureTest<true>;

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiFeatureDisabledTest,
                       IsFeatureEnabled_FeatureDisabled) {
  ASSERT_TRUE(RunSubtest("testFeatureDisabled")) << message_;
}

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiFeatureEnabledTest,
                       IsFeatureEnabled_FeatureEnabled) {
  ASSERT_TRUE(RunSubtest("testFeatureEnabled")) << message_;
}

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiTest, IsFeatureUnknown) {
  ASSERT_TRUE(RunSubtest("testFeatureUnknown")) << message_;
}

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiTest, AcceptConfirmationDialog) {
  ASSERT_TRUE(RunSubtest("testAcceptConfirmationDialog")) << message_;

  // The test has requested to open the confirmation dialog. Check that
  // it was created, then confirm it.
  AccessibilityConfirmationDialog* dialog_ =
      Shell::Get()->accessibility_controller()->GetConfirmationDialogForTest();
  ASSERT_NE(dialog_, nullptr);

  EXPECT_EQ(dialog_->GetWindowTitle(), u"Confirm me! 🐶");

  // Accept the dialog and wait for the JS test to get the confirmation.
  extensions::ResultCatcher catcher;
  dialog_->Accept();
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiTest, CancelConfirmationDialog) {
  ASSERT_TRUE(RunSubtest("testCancelConfirmationDialog")) << message_;

  // The test has requested to open the confirmation dialog. Check that
  // it was created, then cancel it.
  AccessibilityConfirmationDialog* dialog_ =
      Shell::Get()->accessibility_controller()->GetConfirmationDialogForTest();
  ASSERT_NE(dialog_, nullptr);

  EXPECT_EQ(dialog_->GetWindowTitle(), u"Cancel me!");

  // Cancel the dialog and wait for the JS test to get the callback.
  extensions::ResultCatcher catcher;
  dialog_->Cancel();
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiTest, CloseConfirmationDialog) {
  ASSERT_TRUE(RunSubtest("testCancelConfirmationDialog")) << message_;

  // The test has requested to open the confirmation dialog. Check that
  // it was created, then close it.
  AccessibilityConfirmationDialog* dialog_ =
      Shell::Get()->accessibility_controller()->GetConfirmationDialogForTest();
  ASSERT_TRUE(dialog_ != nullptr);

  EXPECT_EQ(dialog_->GetWindowTitle(), u"Cancel me!");

  // Close the dialog and wait for the JS test to get the callback.
  extensions::ResultCatcher catcher;
  dialog_->Close();
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiTest, UpdateDictationBubble) {
  // Enable Dictation to allow the API to work.
  Shell::Get()->accessibility_controller()->dictation().SetEnabled(true);

  // This test requires some back and forth communication between C++ and JS.
  // Use message listeners to force the synchronicity of this test.
  ExtensionTestMessageListener standby_listener("Standby", /*will_reply=*/true);
  ExtensionTestMessageListener show_text_listener("Show text",
                                                  /*will_reply=*/true);
  ExtensionTestMessageListener macro_success_listener("Show macro success",
                                                      /*will_reply=*/true);
  ExtensionTestMessageListener reset_listener("Reset", /*will_reply=*/true);
  ExtensionTestMessageListener hide_listener("Hide", /*will_reply=*/false);

  extensions::ResultCatcher result_catcher;
  ASSERT_TRUE(RunSubtest("testUpdateDictationBubble")) << message_;

  ASSERT_TRUE(standby_listener.WaitUntilSatisfied());
  EXPECT_TRUE(IsDictationBubbleVisible());
  EXPECT_EQ(std::u16string(), GetDictationBubbleText());
  EXPECT_EQ(DictationBubbleIconType::kStandby, GetDictationBubbleVisibleIcon());
  standby_listener.Reply("Continue");

  ASSERT_TRUE(show_text_listener.WaitUntilSatisfied());
  EXPECT_TRUE(IsDictationBubbleVisible());
  EXPECT_EQ(u"Hello", GetDictationBubbleText());
  EXPECT_EQ(DictationBubbleIconType::kHidden, GetDictationBubbleVisibleIcon());
  show_text_listener.Reply("Continue");

  ASSERT_TRUE(macro_success_listener.WaitUntilSatisfied());
  EXPECT_TRUE(IsDictationBubbleVisible());
  EXPECT_EQ(u"Hello", GetDictationBubbleText());
  EXPECT_EQ(DictationBubbleIconType::kMacroSuccess,
            GetDictationBubbleVisibleIcon());
  macro_success_listener.Reply("Continue");

  ASSERT_TRUE(reset_listener.WaitUntilSatisfied());
  EXPECT_TRUE(IsDictationBubbleVisible());
  EXPECT_EQ(std::u16string(), GetDictationBubbleText());
  EXPECT_EQ(DictationBubbleIconType::kStandby, GetDictationBubbleVisibleIcon());
  reset_listener.Reply("Continue");

  ASSERT_TRUE(hide_listener.WaitUntilSatisfied());
  EXPECT_FALSE(IsDictationBubbleVisible());
  EXPECT_EQ(std::u16string(), GetDictationBubbleText());
  EXPECT_EQ(DictationBubbleIconType::kHidden, GetDictationBubbleVisibleIcon());

  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         AccessibilityPrivateApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         AccessibilityPrivateApiFeatureDisabledTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         AccessibilityPrivateApiFeatureEnabledTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         AccessibilityPrivateApiTest,
                         ::testing::Values(ContextType::kServiceWorker));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         AccessibilityPrivateApiFeatureDisabledTest,
                         ::testing::Values(ContextType::kServiceWorker));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         AccessibilityPrivateApiFeatureEnabledTest,
                         ::testing::Values(ContextType::kServiceWorker));

}  // namespace ash

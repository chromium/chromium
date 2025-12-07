// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/ax_main_node_annotator_controller.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/ax_main_node_annotator_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "services/screen_ai/public/cpp/utilities.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_features.mojom-features.h"
#include "ui/accessibility/platform/ax_platform.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/chromevox_test_utils.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/common/constants.h"
#else
#include <optional>

#include "content/public/test/scoped_accessibility_mode_override.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

class AXMainNodeAnnotatorControllerBrowserTest : public InProcessBrowserTest {
 public:
  AXMainNodeAnnotatorControllerBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kMainNodeAnnotations, features::kScreenAITestMode,
         ax::mojom::features::kScreenAIMainContentExtractionEnabled},
        {});
  }
  ~AXMainNodeAnnotatorControllerBrowserTest() override = default;

  // InProcessBrowserTest overrides:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

#if BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS)
    screen_ai::ScreenAIInstallState::GetInstance()->SetComponentFolder(
        screen_ai::GetComponentBinaryPathForTests().DirName());
#endif

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }
  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
    EnableScreenReader(false);
  }

  void Connect() {
#if BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS)
    base::test::TestFuture<bool> future;
    screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(
        browser()->profile())
        ->GetServiceStateAsync(
            screen_ai::ScreenAIServiceRouter::Service::kMainContentExtraction,
            future.GetCallback());
    ASSERT_TRUE(future.Wait()) << "Service state callback not called.";
    ASSERT_TRUE(future.Get<bool>()) << "Service initialization failed.";
#else
    screen_ai::AXMainNodeAnnotatorControllerFactory::GetForProfile(
        browser()->profile())
        ->set_service_ready_for_testing();
#endif
  }

  void CompleteServiceInitialization() {
    screen_ai::AXMainNodeAnnotatorControllerFactory::GetForProfile(
        browser()->profile())
        ->complete_service_intialization_for_testing();
  }

  void EnableScreenReader(bool enabled) {
#if BUILDFLAG(IS_CHROMEOS)
    if (!enabled) {
      ash::AccessibilityManager::Get()->EnableSpokenFeedback(false);
      return;
    }

    if (!chromevox_test_utils_) {
      chromevox_test_utils_ = std::make_unique<ash::ChromeVoxTestUtils>();
    }

    chromevox_test_utils_->EnableChromeVox();
    // Note: we can safely call `Replay` here since none of these tests make
    // speech assertions.
    chromevox_test_utils_->sm()->Replay();
#else
    // Spoof a screen reader.
    if (!enabled) {
      ax_mode_override_.reset();
    } else if (!ax_mode_override_) {
      ax_mode_override_.emplace(ui::AXMode::kWebContents |
                                ui::AXMode::kExtendedProperties |
                                ui::AXMode::kScreenReader);
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

 private:
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<ash::ChromeVoxTestUtils> chromevox_test_utils_;
#else
  std::optional<content::ScopedAccessibilityModeOverride> ax_mode_override_;
#endif

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Changing the kAccessibilityMainNodeAnnotationsEnabled pref should affect the
// accessibility mode of a new WebContents for this profile.
IN_PROC_BROWSER_TEST_F(AXMainNodeAnnotatorControllerBrowserTest,
                       NewWebContents) {
  Connect();
  EnableScreenReader(true);
  ui::AXMode ax_mode =
      content::BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kAnnotateMainNode));

  chrome::NewTab(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ax_mode = web_contents->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kAnnotateMainNode));

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityMainNodeAnnotationsEnabled, true);

  chrome::NewTab(browser());
  web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  // Wait for ChromeVox to attach to the new tab if needed.
  if (!web_contents->GetAccessibilityMode().has_mode(
          ui::AXMode::kExtendedProperties)) {
    content::AccessibilityNotificationWaiter waiter(web_contents);
    ASSERT_TRUE(waiter.WaitForNotification());
  }
  ax_mode = web_contents->GetAccessibilityMode();
  EXPECT_TRUE(ax_mode.has_mode(ui::AXMode::kAnnotateMainNode));

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityMainNodeAnnotationsEnabled, false);

  chrome::NewTab(browser());
  web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ax_mode = web_contents->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kAnnotateMainNode));
}

// Changing the kAccessibilityMainNodeAnnotationsEnabled pref should affect the
// accessibility mode of existing WebContents in this profile.
IN_PROC_BROWSER_TEST_F(AXMainNodeAnnotatorControllerBrowserTest,
                       ExistingWebContents) {
  Connect();
  EnableScreenReader(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui::AXMode ax_mode = web_contents->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kAnnotateMainNode));

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityMainNodeAnnotationsEnabled, true);

  ax_mode = web_contents->GetAccessibilityMode();
  EXPECT_TRUE(ax_mode.has_mode(ui::AXMode::kAnnotateMainNode));

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityMainNodeAnnotationsEnabled, false);

  ax_mode = web_contents->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kAnnotateMainNode));
}

IN_PROC_BROWSER_TEST_F(AXMainNodeAnnotatorControllerBrowserTest,
                       NotEnabledWithoutScreenReader) {
  Connect();
  EnableScreenReader(false);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui::AXMode ax_mode = web_contents->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kAnnotateMainNode));

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityMainNodeAnnotationsEnabled, true);

  ax_mode = web_contents->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kAnnotateMainNode));

  // Reset state.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityMainNodeAnnotationsEnabled, false);
}

// Turning on the preference while a screenreader is present should enable the
// feature for existing tabs.
IN_PROC_BROWSER_TEST_F(AXMainNodeAnnotatorControllerBrowserTest,
                       PRE_EnabledByPreference) {
  Connect();
  EnableScreenReader(true);

  // The preference is not yet set, so the feature is off.
  auto* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(web_contents->GetAccessibilityMode().has_mode(
      ui::AXMode::kAnnotateMainNode));

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityMainNodeAnnotationsEnabled, true);

  // Now the feature is on.
  EXPECT_TRUE(web_contents->GetAccessibilityMode().has_mode(
      ui::AXMode::kAnnotateMainNode));
}

// When the preference is present at startup, the feature should become enabled
// when a screenreader is discovered.
IN_PROC_BROWSER_TEST_F(AXMainNodeAnnotatorControllerBrowserTest,
                       EnabledByPreference) {
  // If the test is run with --force-renderer-accessibility, then initializing
  // the class causes the service to kick off. We need to force it to complete.
  bool screen_reader = ui::AXPlatform::GetInstance().IsScreenReaderActive();
  if (screen_reader) {
    CompleteServiceInitialization();
  } else {
    Connect();
  }

  // The preference was set for the profile by PRE_EnabledByPreference.
  ASSERT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kAccessibilityMainNodeAnnotationsEnabled));

  auto* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // If the test is run without --force-renderer-accessibility, then no screen
  // reader should have been detected yet, and the feature should be off.
  if (!screen_reader) {
    EXPECT_FALSE(web_contents->GetAccessibilityMode().has_mode(
        ui::AXMode::kAnnotateMainNode));
    EnableScreenReader(true);
  }

  // Now the feature is on.
  EXPECT_TRUE(web_contents->GetAccessibilityMode().has_mode(
      ui::AXMode::kAnnotateMainNode));
}

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/strings/pattern.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/events/test/event_generator.h"
#include "url/url_constants.h"

namespace ash {

class AccessibilityLiveSiteTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    ASSERT_FALSE(AccessibilityManager::Get()->IsSelectToSpeakEnabled());

    extensions::ExtensionHostTestHelper host_helper(browser()->profile());
    AccessibilityManager::Get()->SetSelectToSpeakEnabled(true);
    host_helper.WaitForHostCompletedFirstLoad();

    aura::Window* root_window = Shell::Get()->GetPrimaryRootWindow();
    generator_ = std::make_unique<ui::test::EventGenerator>(root_window);

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  }

  void SetUpInProcessBrowserTestFixture() override {
    // To avoid depending on external resources, browser tests don't allow
    // non-local DNS queries by default. Override this for this specific manual
    // test suite.
    host_resolver()->AllowDirectLookup("*.google.com");
    host_resolver()->AllowDirectLookup("*.gstatic.com");

    // Pretend that enhanced network voices dialog has been accepted so that the
    // dialog does not block.
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kAccessibilitySelectToSpeakEnhancedVoicesDialogShown, true);

    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  test::SpeechMonitor speech_monitor_;
  std::unique_ptr<ui::test::EventGenerator> generator_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This is a sanity check / integration test that Select-to-speak works
// with Google Docs, since we have a small amount of code that works
// around a compatibility issue.
//
// It's only run on an FYI bot because we don't want Docs outages to affect the
// Chrome waterfall.
//
// To visually see what's happening while the test is running,
// add this option:
//    --enable-pixel-output-in-tests
IN_PROC_BROWSER_TEST_F(AccessibilityLiveSiteTest,
                       SelectToSpeakGoogleDocsSupport) {
  const char* kGoogleDocsUrl =
      "https://docs.google.com/document/d/"
      "1qpu3koSIHpBzQbxeEE-dofSKXCIgdc4yJLI-o1LpCPs/view";
  const char* kTextFoundInGoogleDoc = "Long-string-to-test-select-to-speak";

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kGoogleDocsUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::ScopedAccessibilityModeOverride scoped_accessibility_mode(
      web_contents, ui::kAXModeComplete);

  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_contents, "Long-string-to-test-select-to-speak");
  gfx::Rect bounds = browser()->window()->GetBounds();
  generator_->PressKey(ui::VKEY_LWIN, 0 /* flags */);
  generator_->MoveMouseTo(bounds.x() + 8, bounds.y() + 200);
  generator_->PressLeftButton();
  generator_->MoveMouseTo(bounds.x() + bounds.width() - 8,
                          bounds.y() + bounds.height() - 8);
  generator_->ReleaseLeftButton();
  generator_->ReleaseKey(ui::VKEY_LWIN, 0 /* flags */);

  speech_monitor_.ExpectSpeech(kTextFoundInGoogleDoc);
  speech_monitor_.Replay();
}

}  // namespace ash

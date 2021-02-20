// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/notification_types.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/events/test/event_generator.h"
#include "url/url_constants.h"

namespace ash {

class AccessibilityLiveSiteTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    ASSERT_FALSE(AccessibilityManager::Get()->IsSelectToSpeakEnabled());

    content::WindowedNotificationObserver extension_load_waiter(
        extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD,
        content::NotificationService::AllSources());
    AccessibilityManager::Get()->SetSelectToSpeakEnabled(true);
    extension_load_waiter.Wait();

    aura::Window* root_window = Shell::Get()->GetPrimaryRootWindow();
    generator_.reset(new ui::test::EventGenerator(root_window));

    ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  }

  void SetUpInProcessBrowserTestFixture() override {
    // TODO: this logic currently doesn't work and the test never passes due to
    // inability to lookup docs.google.com. To avoid depending on external
    // resources, browser tests don't allow non-local DNS queries by default.
    // Override this for this specific manual test suite.
    scoped_refptr<net::RuleBasedHostResolverProc> resolver =
        new net::RuleBasedHostResolverProc(host_resolver());
    resolver->AllowDirectLookup("*.google.com");
    resolver->AllowDirectLookup("*.gstatic.com");
    mock_host_resolver_override_.reset(
        new net::ScopedDefaultHostResolverProc(resolver.get()));
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_host_resolver_override_.reset();
  }

  test::SpeechMonitor speech_monitor_;
  std::unique_ptr<ui::test::EventGenerator> generator_;

  std::unique_ptr<net::ScopedDefaultHostResolverProc>
      mock_host_resolver_override_;
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

  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();

  ui_test_utils::NavigateToURL(browser(), GURL(kGoogleDocsUrl));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::EnableAccessibilityForWebContents(web_contents);

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

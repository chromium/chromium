// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/accessibility/uia_accessibility_event_waiter.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

#include <uiautomation.h>

namespace {

// Test class for verifying that AriaNotify triggers UIA_NotificationEventId.
class AriaNotifyUIABrowserTest : public InProcessBrowserTest {
 public:
  AriaNotifyUIABrowserTest() {
    // Enable both AriaNotify (for the web API) and UiaProvider (so that
    // UIA notification events are fired instead of using the IA2 fallback).
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kAriaNotify,
                              ::features::kUiaProvider},
        {});
  }

  AriaNotifyUIABrowserTest(const AriaNotifyUIABrowserTest&) = delete;
  AriaNotifyUIABrowserTest& operator=(const AriaNotifyUIABrowserTest&) = delete;

  ~AriaNotifyUIABrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    scoped_accessibility_mode_.emplace(GetWebContents(), ui::kAXModeComplete);
  }

  void TearDownOnMainThread() override { scoped_accessibility_mode_.reset(); }

  content::WebContents* GetWebContents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  HWND GetWebPageHwnd() const {
    return browser()
        ->window()
        ->GetNativeWindow()
        ->GetHost()
        ->GetAcceleratedWidget();
  }

  void NavigateToHtml(const std::string& html) {
    GURL url("data:text/html," + html);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::optional<content::ScopedAccessibilityModeOverride>
      scoped_accessibility_mode_;
};

// Tests that calling ariaNotify raises a UIA_NotificationEventId event.
// This test uses the uia_event_id field to wait directly for the UIA
// notification event instead of mapping from ax::mojom::Event.
IN_PROC_BROWSER_TEST_F(AriaNotifyUIABrowserTest,
                       AriaNotifyRaisesUIANotificationEvent) {
  // Create the waiter BEFORE navigation to ensure we don't miss kLoadComplete.
  content::AccessibilityNotificationWaiter tree_waiter(
      GetWebContents(), ax::mojom::Event::kLoadComplete);

  // Load a page with a button that triggers ariaNotify.
  NavigateToHtml(R"HTML(
    <!DOCTYPE html>
    <html>
    <body>
      <button id="notifyButton" aria-label="Notification Trigger">
        Trigger Notification
      </button>
      <script>
        document.getElementById('notifyButton').addEventListener('click',
          function() {
            this.ariaNotify('Test notification message');
          });
      </script>
    </body>
    </html>
  )HTML");

  // Wait for the accessibility tree to be ready.
  ASSERT_TRUE(tree_waiter.WaitForNotification());

  // Set up the UIA event waiter for UIA_NotificationEventId.
  // NOTE: The waiter must be created AFTER the accessibility tree is ready,
  // because the UIA event subscriptions can interfere with tree initialization
  // and cause hangs on Windows.
  UiaAccessibilityWaiterInfo info = {GetWebPageHwnd(),
                                     /*role=*/L"button",
                                     /*name=*/L"Notification Trigger",
                                     /*event=*/ax::mojom::Event::kNone,
                                     /*uia_event_id=*/UIA_NotificationEventId};
  UiaAccessibilityEventWaiter waiter(info);

  // Click the button to trigger ariaNotify.
  ASSERT_TRUE(ExecJs(GetWebContents(),
                     "document.getElementById('notifyButton').click();"));

  // Wait for the UIA notification event to be raised.
  waiter.Wait();

  // Verify the notification string was captured correctly.
  EXPECT_EQ(waiter.GetNotificationString(), L"Test notification message");
}

}  // namespace

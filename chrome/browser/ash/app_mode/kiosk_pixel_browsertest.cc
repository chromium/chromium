// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"

namespace ash {

namespace {

bool IsPixelTestEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kVerifyPixels);
}

// Helper class to wait until contents finish loading, either on failure or
// success.
class LoadWaiter final : public content::WebContentsObserver {
 public:
  explicit LoadWaiter(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~LoadWaiter() override = default;

  // Blocks until `web_contents` has finished loading.
  [[nodiscard]] bool Wait() { return signal_.Wait(); }

 private:
  // Unblocks any callers currently waiting on `Wait()`.
  void UnblockCallers() {
    if (!signal_.IsReady()) {
      signal_.SetValue();
    }
  }

  // WebContentsObserver override.
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    UnblockCallers();
  }

  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override {
    UnblockCallers();
  }

  base::test::TestFuture<void> signal_;
};

// Blocks until the given WebContents finishes loading.
void AwaitContentsLoaded(content::WebContents* contents) {
  ASSERT_NE(contents, nullptr);
  ASSERT_TRUE(LoadWaiter(contents).Wait()) << "Timedout loading contents";
}

// Uses Skia Gold API to compare pixels of the given Browser WebContents.
void VerifyBrowserContents(Browser const* browser,
                           const std::string& screenshot_name) {
  ASSERT_NE(browser, nullptr);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  ASSERT_NE(browser_view, nullptr);
  ContentsWebView* contents_view = browser_view->contents_web_view();
  ASSERT_NE(contents_view, nullptr);

  AwaitContentsLoaded(contents_view->web_contents());

  views::ViewSkiaGoldPixelDiff pixel_diff(
      ::testing::UnitTest::GetInstance()->current_test_suite()->name());
  EXPECT_TRUE(pixel_diff.CompareViewScreenshot(screenshot_name, contents_view));
}

}  // namespace

class KioskPixelTest : public WebKioskBaseTest {
 public:
  // Cursor hover may change styles of items in the screenshot. Move the cursor
  // to a corner where it won't interfere with anything.
  void MoveCursorToCorner() {
    if (!browser()) {
      SelectFirstBrowser();
    }
    EXPECT_NE(browser(), nullptr) << "browser() is null, can't move cursor.";
    auto event_generator = std::make_unique<ui::test::EventGenerator>(
        browser()->window()->GetNativeWindow()->GetRootWindow());
    event_generator->MoveMouseTo(10, 10);
  }
};

// TODO(b/278898101): disable due to flakiness.
IN_PROC_BROWSER_TEST_F(KioskPixelTest, DISABLED_AccessibilitySettings) {
  if (!IsPixelTestEnabled()) {
    return;
  }

  InitializeRegularOnlineKiosk();
  ASSERT_NE(WebKioskAppManager::Get()->kiosk_system_session(), nullptr);
  Browser const* settings_browser = OpenA11ySettingsBrowser(
      WebKioskAppManager::Get()->kiosk_system_session());
  MoveCursorToCorner();
  VerifyBrowserContents(settings_browser, "AccessibilitySettings_rev0");
}

}  // namespace ash

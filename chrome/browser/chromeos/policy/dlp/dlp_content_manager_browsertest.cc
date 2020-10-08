// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"

#include "chrome/browser/ui/ash/screenshot_area.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"

namespace policy {

namespace {
const DlpContentRestrictionSet kScreenshotRestricted(
    DlpContentRestriction::kScreenshot);
}  // namespace

class DlpContentManagerBrowserTest : public InProcessBrowserTest {
 public:
  DlpContentManagerBrowserTest() {}
};

IN_PROC_BROWSER_TEST_F(DlpContentManagerBrowserTest, ScreenshotsRestricted) {
  DlpContentManager* manager = DlpContentManager::Get();
  ui_test_utils::NavigateToURL(browser(), GURL("https://example.com"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();
  ScreenshotArea fullscreen = ScreenshotArea::CreateForAllRootWindows();
  ScreenshotArea window = ScreenshotArea::CreateForWindow(root_window);
  const gfx::Rect web_contents_rect = web_contents->GetContainerBounds();
  gfx::Rect out_rect(web_contents_rect);
  out_rect.Offset(web_contents_rect.width(), web_contents_rect.height());
  gfx::Rect in_rect(web_contents_rect);
  in_rect.Offset(web_contents_rect.width() / 2, web_contents_rect.height() / 2);
  ScreenshotArea partial_out =
      ScreenshotArea::CreateForPartialWindow(root_window, out_rect);
  ScreenshotArea partial_in =
      ScreenshotArea::CreateForPartialWindow(root_window, in_rect);

  EXPECT_FALSE(manager->IsScreenshotRestricted(fullscreen));
  EXPECT_FALSE(manager->IsScreenshotRestricted(window));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_out));

  manager->OnConfidentialityChanged(web_contents, kScreenshotRestricted);
  EXPECT_TRUE(manager->IsScreenshotRestricted(fullscreen));
  EXPECT_TRUE(manager->IsScreenshotRestricted(window));
  EXPECT_TRUE(manager->IsScreenshotRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_out));

  web_contents->WasHidden();
  manager->OnVisibilityChanged(web_contents);
  EXPECT_FALSE(manager->IsScreenshotRestricted(fullscreen));
  EXPECT_TRUE(manager->IsScreenshotRestricted(window));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_out));

  web_contents->WasShown();
  manager->OnVisibilityChanged(web_contents);
  EXPECT_TRUE(manager->IsScreenshotRestricted(fullscreen));
  EXPECT_TRUE(manager->IsScreenshotRestricted(window));
  EXPECT_TRUE(manager->IsScreenshotRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_out));

  manager->OnWebContentsDestroyed(web_contents);
  EXPECT_FALSE(manager->IsScreenshotRestricted(fullscreen));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_out));
}

}  // namespace policy

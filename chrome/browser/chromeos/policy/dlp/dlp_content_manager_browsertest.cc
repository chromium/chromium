// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"

#include "chrome/browser/ui/ash/screenshot_area.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
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
const DlpContentRestrictionSet kVideoCaptureRestricted(
    DlpContentRestriction::kVideoCapture);
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
  ScreenshotArea window =
      ScreenshotArea::CreateForWindow(web_contents->GetNativeView());
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

IN_PROC_BROWSER_TEST_F(DlpContentManagerBrowserTest,
                       VideoCaptureStoppedWhenConfidentialWindowResized) {
  DlpContentManager* manager = DlpContentManager::Get();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();
  ScreenshotArea fullscreen = ScreenshotArea::CreateForPartialWindow(
      root_window, root_window->bounds());

  // Open first browser window.
  Browser* browser1 = browser();
  chrome::NewTab(browser1);
  ui_test_utils::NavigateToURL(browser1, GURL("https://example.com"));
  content::WebContents* web_contents1 =
      browser1->tab_strip_model()->GetActiveWebContents();

  // Open second browser window.
  Browser* browser2 =
      new Browser(Browser::CreateParams(browser()->profile(), true));
  chrome::NewTab(browser2);
  ui_test_utils::NavigateToURL(browser2, GURL("https://google.com"));

  // Resize browsers so that second window covers the first one.
  // Browser window can't have width less than 500.
  browser1->window()->SetBounds(gfx::Rect(100, 100, 500, 500));
  browser2->window()->SetBounds(gfx::Rect(0, 0, 700, 700));

  // Make first window content as confidential.
  manager->OnConfidentialityChanged(web_contents1, kVideoCaptureRestricted);

  // Start capture of the whole screen.
  base::RunLoop run_loop;
  manager->OnVideoCaptureStarted(fullscreen, run_loop.QuitClosure());

  // Move first window with confidential content to make it visible.
  browser1->window()->SetBounds(gfx::Rect(100, 100, 700, 700));

  // Check that capture was requested to be stopped via callback.
  run_loop.Run();

  manager->OnVideoCaptureStopped();
  browser2->window()->Close();
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerBrowserTest,
                       VideoCaptureStoppedWhenNonConfidentialWindowResized) {
  DlpContentManager* manager = DlpContentManager::Get();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();
  ScreenshotArea fullscreen = ScreenshotArea::CreateForPartialWindow(
      root_window, root_window->bounds());

  // Open first browser window.
  Browser* browser1 = browser();
  chrome::NewTab(browser1);
  ui_test_utils::NavigateToURL(browser1, GURL("https://example.com"));
  content::WebContents* web_contents1 =
      browser1->tab_strip_model()->GetActiveWebContents();

  // Open second browser window.
  Browser* browser2 =
      new Browser(Browser::CreateParams(browser()->profile(), true));
  chrome::NewTab(browser2);
  ui_test_utils::NavigateToURL(browser2, GURL("https://google.com"));

  // Resize browsers so that second window covers the first one.
  // Browser window can't have width less than 500.
  browser1->window()->SetBounds(gfx::Rect(100, 100, 500, 500));
  browser2->window()->SetBounds(gfx::Rect(0, 0, 700, 700));

  // Make first window content as confidential.
  manager->OnConfidentialityChanged(web_contents1, kVideoCaptureRestricted);

  // Start capture of the whole screen.
  base::RunLoop run_loop;
  manager->OnVideoCaptureStarted(fullscreen, run_loop.QuitClosure());

  // Move second window to make first window with confidential content visible.
  browser2->window()->SetBounds(gfx::Rect(150, 150, 700, 700));

  // Check that capture was requested to be stopped via callback.
  run_loop.Run();

  manager->OnVideoCaptureStopped();
  browser2->window()->Close();
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerBrowserTest,
                       VideoCaptureNotStoppedWhenConfidentialWindowHidden) {
  DlpContentManager* manager = DlpContentManager::Get();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();
  ScreenshotArea fullscreen = ScreenshotArea::CreateForPartialWindow(
      root_window, root_window->bounds());

  // Open first browser window.
  Browser* browser1 = browser();
  chrome::NewTab(browser1);
  ui_test_utils::NavigateToURL(browser1, GURL("https://example.com"));
  content::WebContents* web_contents1 =
      browser1->tab_strip_model()->GetActiveWebContents();

  // Open second browser window.
  Browser* browser2 =
      new Browser(Browser::CreateParams(browser()->profile(), true));
  chrome::NewTab(browser2);
  ui_test_utils::NavigateToURL(browser2, GURL("https://google.com"));

  // Resize browsers so that second window covers the first one.
  // Browser window can't have width less than 500.
  browser1->window()->SetBounds(gfx::Rect(100, 100, 500, 500));
  browser2->window()->SetBounds(gfx::Rect(0, 0, 700, 700));

  // Make first window content as confidential.
  manager->OnConfidentialityChanged(web_contents1, kVideoCaptureRestricted);

  // Start capture of the whole screen.
  base::RunLoop run_loop;
  manager->OnVideoCaptureStarted(
      fullscreen, base::BindOnce([] {
        FAIL() << "Video capture stop callback shouldn't be called";
      }));

  // Move first window, but keep confidential content hidden.
  browser1->window()->SetBounds(gfx::Rect(150, 150, 500, 500));

  // Check that capture was not requested to be stopped via callback.
  manager->OnVideoCaptureStopped();
  run_loop.RunUntilIdle();

  browser2->window()->Close();
}

}  // namespace policy

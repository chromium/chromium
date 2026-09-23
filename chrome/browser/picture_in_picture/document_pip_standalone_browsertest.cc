// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_path_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/screen.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace {

using content::EvalJs;

constexpr base::FilePath::CharType kDocumentPipPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/document-pip.html");

}  // namespace

// Base fixture for Document PiP browser tests.
class DocumentPipStandaloneBrowserTestBase : public InProcessBrowserTest {
 public:
  DocumentPipStandaloneBrowserTestBase() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* OpenerWebContents() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

  DocumentPipHost* GetDocumentPipHost() {
    return DocumentPipHost::FromWebContents(OpenerWebContents());
  }

  // Navigates the active tab to the test page and opens a Document PiP window.
  void OpenDocumentPipWindow() {
    GURL url = chrome_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kDocumentPipPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    ASSERT_EQ(true,
              EvalJs(OpenerWebContents(),
                     "createDocumentPipWindow({width: 400, height: 300})"));
  }
};

// Fixture with the standalone Document PiP path enabled.
class DocumentPipStandaloneEnabledBrowserTest
    : public DocumentPipStandaloneBrowserTestBase {
 public:
  DocumentPipStandaloneEnabledBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kDocumentPictureInPictureAPI,
         features::kDocumentPipStandaloneWindow},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class DocumentPipLifecycleBrowserTest
    : public DocumentPipStandaloneBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  DocumentPipLifecycleBrowserTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{blink::features::kDocumentPictureInPictureAPI, true},
         {features::kDocumentPipStandaloneWindow, GetParam()}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         DocumentPipLifecycleBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Standalone" : "BrowserBacked";
                         });

// The standalone PiP widget should be placed on the same display as its
// opener (regression guard for: the widget is given only a size, so the origin
// defaults to the primary display's top-left).
//
// LIMITATION: browser_tests run with a single display (Xvfb on Linux, the host
// display on Mac), so the opener display is the primary the perhaps the only
// display and this assertion is trivially true even in the regressed state. It
// therefore cannot actually catch the regression it guards against in CI.
// TODO(crbug.com/515252142): Migrate to interactive_ui_tests and use
// display::test::VirtualDisplayUtil to place the opener on a non-primary
// display, then assert the PiP window lands there rather than on the primary
// display.
IN_PROC_BROWSER_TEST_F(DocumentPipStandaloneEnabledBrowserTest,
                       OpensOnSameDisplayAsOpener) {
  OpenDocumentPipWindow();

  auto* host = GetDocumentPipHost();
  ASSERT_NE(nullptr, host);
  ASSERT_NE(nullptr, host->GetWidget());
  ASSERT_TRUE(host->GetWidget()->IsVisible());

  const display::Screen* const screen = display::Screen::Get();
  const display::Display opener_display = screen->GetDisplayNearestView(
      OpenerWebContents()->GetContentNativeView());
  const display::Display pip_display =
      screen->GetDisplayNearestWindow(host->GetWidget()->GetNativeWindow());

  EXPECT_EQ(opener_display.id(), pip_display.id());
}

IN_PROC_BROWSER_TEST_P(DocumentPipLifecycleBrowserTest, ExitClosesWindow) {
  const bool is_standalone = GetParam();
  ASSERT_NO_FATAL_FAILURE(OpenDocumentPipWindow());

  auto* opener = OpenerWebContents();
  auto* manager = PictureInPictureWindowManager::GetInstance();
  ASSERT_TRUE(manager->IsInPictureInPicture());
  ASSERT_EQ(opener, manager->GetWebContents());
  auto* child = manager->GetChildWebContents();
  ASSERT_NE(nullptr, child);
  auto* widget =
      views::Widget::GetWidgetForNativeWindow(child->GetTopLevelNativeWindow());
  ASSERT_NE(nullptr, widget);
  content::WebContentsDestroyedWatcher child_destroyed_watcher(child);
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(widget);
  auto* host = GetDocumentPipHost();
  ASSERT_EQ(is_standalone, host != nullptr);
  base::WeakPtr<DocumentPipHost> host_weak =
      host ? host->GetWeakPtr() : nullptr;

  ASSERT_TRUE(manager->ExitPictureInPicture());

  child_destroyed_watcher.Wait();
  widget_destroyed_waiter.Wait();
  EXPECT_FALSE(manager->IsInPictureInPicture());
  EXPECT_EQ(nullptr, manager->GetWebContents());
  EXPECT_EQ(nullptr, manager->GetChildWebContents());
  if (is_standalone) {
    ASSERT_TRUE(host_weak);
    EXPECT_EQ(host_weak.get(), GetDocumentPipHost());
    EXPECT_EQ(nullptr, host_weak->GetWidget());
    EXPECT_EQ(nullptr, host_weak->GetChildWebContents());
  }
}

// Regression test for crbug.com/544038274: a renderer-initiated same-window
// navigation from the PiP child must not synchronously destroy the
// child WebContents while content::WebContentsImpl::OpenURL() is still using
// source-frame state on the stack.
IN_PROC_BROWSER_TEST_P(DocumentPipLifecycleBrowserTest,
                       ChildCurrentTabNavigationClosesWindowWithoutCrashing) {
  const bool is_standalone = GetParam();
  ASSERT_NO_FATAL_FAILURE(OpenDocumentPipWindow());

  auto* manager = PictureInPictureWindowManager::GetInstance();
  content::WebContents* child = manager->GetChildWebContents();
  ASSERT_NE(nullptr, child);
  ASSERT_NE(OpenerWebContents(), child);
  auto* widget =
      views::Widget::GetWidgetForNativeWindow(child->GetTopLevelNativeWindow());
  ASSERT_NE(nullptr, widget);
  content::WebContentsDestroyedWatcher child_destroyed_watcher(child);
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(widget);
  auto* host = GetDocumentPipHost();
  ASSERT_EQ(is_standalone, host != nullptr);
  base::WeakPtr<DocumentPipHost> host_weak =
      host ? host->GetWeakPtr() : nullptr;

  // Exercise OpenURLFromTab rather than the usual BeginNavigation path.
  child->GetMutableRendererPrefs()->browser_handles_all_top_level_requests =
      true;
  child->SyncRendererPrefs();

  content::ExecuteScriptAsync(
      child, content::JsReplace("location.href = $1;",
                                embedded_test_server()->GetURL(
                                    "example.test", "/title1.html")));

  child_destroyed_watcher.Wait();
  widget_destroyed_waiter.Wait();

  EXPECT_EQ(nullptr, manager->GetChildWebContents());
  if (is_standalone) {
    ASSERT_TRUE(host_weak);
    EXPECT_EQ(nullptr, host_weak->GetWidget());
    EXPECT_EQ(nullptr, host_weak->GetChildWebContents());
  }
}

IN_PROC_BROWSER_TEST_P(DocumentPipLifecycleBrowserTest,
                       OpenerDestroyedClosesWindow) {
  const bool is_standalone = GetParam();
  ASSERT_NO_FATAL_FAILURE(OpenDocumentPipWindow());

  auto* opener = OpenerWebContents();
  auto* manager = PictureInPictureWindowManager::GetInstance();
  ASSERT_TRUE(manager->IsInPictureInPicture());
  ASSERT_EQ(opener, manager->GetWebContents());
  auto* child = manager->GetChildWebContents();
  ASSERT_NE(nullptr, child);
  auto* widget =
      views::Widget::GetWidgetForNativeWindow(child->GetTopLevelNativeWindow());
  ASSERT_NE(nullptr, widget);
  content::WebContentsDestroyedWatcher child_destroyed_watcher(child);
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(widget);
  auto* host = GetDocumentPipHost();
  ASSERT_EQ(is_standalone, host != nullptr);
  base::WeakPtr<DocumentPipHost> host_weak =
      host ? host->GetWeakPtr() : nullptr;

  // Open a second tab so closing the first one doesn't close the browser.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/title1.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  browser()->GetTabStripModel()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);

  child_destroyed_watcher.Wait();
  widget_destroyed_waiter.Wait();
  EXPECT_FALSE(manager->IsInPictureInPicture());
  EXPECT_EQ(nullptr, manager->GetWebContents());
  EXPECT_EQ(nullptr, manager->GetChildWebContents());
  if (is_standalone) {
    EXPECT_FALSE(host_weak);
  }
}

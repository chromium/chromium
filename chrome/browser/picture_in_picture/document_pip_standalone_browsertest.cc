// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
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
#include "third_party/blink/public/common/features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace {

using content::EvalJs;

constexpr base::FilePath::CharType kDocumentPipPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/document-pip.html");

// Counts the open Browser-backed Picture-in-Picture windows by iterating the
// global browser collection. Avoids the FindBrowserWith* helpers, which are
// discouraged for new code.
int CountPictureInPictureBrowsers() {
  int count = 0;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&count](BrowserWindowInterface* browser) {
        if (browser->GetType() ==
            BrowserWindowInterface::TYPE_PICTURE_IN_PICTURE) {
          ++count;
        }
        return true;
      });
  return count;
}

}  // namespace

// Base fixture: enables the Document Picture-in-Picture web API and serves the
// test page. The standalone feature is toggled by the derived fixtures.
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

// Opening a Document PiP window routes to a DocumentPipHost (no Browser) and
// the window is not part of BrowserList.
IN_PROC_BROWSER_TEST_F(DocumentPipStandaloneEnabledBrowserTest,
                       RequestWindowOpensStandaloneHost) {
  OpenDocumentPipWindow();

  auto* host = GetDocumentPipHost();
  ASSERT_NE(nullptr, host);
  EXPECT_NE(nullptr, host->GetWidget());
  EXPECT_NE(nullptr, host->GetChildWebContents());
  EXPECT_NE(OpenerWebContents(), host->GetChildWebContents());

  auto* manager = PictureInPictureWindowManager::GetInstance();
  EXPECT_EQ(host->GetChildWebContents(), manager->GetChildWebContents());
  EXPECT_TRUE(PictureInPictureWindowManager::IsChildWebContents(
      host->GetChildWebContents()));

  // The standalone path must not create a Browser-backed PiP window.
  EXPECT_EQ(0, CountPictureInPictureBrowsers());
}

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

// Exiting via the manager tears down the standalone window and child contents.
IN_PROC_BROWSER_TEST_F(DocumentPipStandaloneEnabledBrowserTest,
                       ExitClosesStandaloneWindow) {
  OpenDocumentPipWindow();

  auto* host = GetDocumentPipHost();
  ASSERT_NE(nullptr, host);
  ASSERT_NE(nullptr, host->GetChildWebContents());

  auto* manager = PictureInPictureWindowManager::GetInstance();
  EXPECT_TRUE(manager->ExitPictureInPicture());

  EXPECT_EQ(nullptr, host->GetWidget());
  EXPECT_EQ(nullptr, host->GetChildWebContents());
  EXPECT_EQ(nullptr, manager->GetChildWebContents());
}

// Navigating the opener to a new page closes the standalone PiP window.
IN_PROC_BROWSER_TEST_F(DocumentPipStandaloneEnabledBrowserTest,
                       OpenerNavigationClosesWindow) {
  OpenDocumentPipWindow();

  auto* host = GetDocumentPipHost();
  ASSERT_NE(nullptr, host);
  ASSERT_NE(nullptr, host->GetWidget());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  // The host stays attached to the opener, but its window is closed.
  EXPECT_EQ(nullptr, host->GetWidget());
  EXPECT_EQ(nullptr, host->GetChildWebContents());
}

// Regression test for crbug.com/544038274: a renderer-initiated same-window
// navigation from the standalone PiP child must not synchronously destroy the
// child WebContents while content::WebContentsImpl::OpenURL() is still using
// source-frame state on the stack.
IN_PROC_BROWSER_TEST_F(DocumentPipStandaloneEnabledBrowserTest,
                       ChildCurrentTabNavigationClosesWindowWithoutCrashing) {
  OpenDocumentPipWindow();

  auto* host = GetDocumentPipHost();
  ASSERT_NE(nullptr, host);
  content::WebContents* child = host->GetChildWebContents();
  ASSERT_NE(nullptr, child);
  content::WebContentsDestroyedWatcher child_destroyed_watcher(child);

  content::ExecuteScriptAsync(
      child, content::JsReplace("location.href = $1;",
                                embedded_test_server()->GetURL(
                                    "example.test", "/title1.html")));

  child_destroyed_watcher.Wait();

  EXPECT_EQ(nullptr, host->GetWidget());
  EXPECT_EQ(nullptr, host->GetChildWebContents());
  EXPECT_EQ(
      nullptr,
      PictureInPictureWindowManager::GetInstance()->GetChildWebContents());
}

// Closing the opener tab destroys the host and leaves no PiP window behind.
IN_PROC_BROWSER_TEST_F(DocumentPipStandaloneEnabledBrowserTest,
                       OpenerDestroyedClosesWindow) {
  OpenDocumentPipWindow();
  auto* host = GetDocumentPipHost();
  ASSERT_NE(nullptr, host);

  // The host is WebContentsUserData on the opener, so it is destroyed when the
  // opener tab closes. Track it with a WeakPtr to verify it is released.
  base::WeakPtr<DocumentPipHost> host_weak = host->GetWeakPtr();

  // Open a second tab so closing the first one doesn't close the browser.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/title1.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  browser()->GetTabStripModel()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);

  // Destroying the opener releases the host and leaves no PiP window behind.
  EXPECT_FALSE(host_weak);
  EXPECT_EQ(
      nullptr,
      PictureInPictureWindowManager::GetInstance()->GetChildWebContents());
}

// Fixture with the standalone Document PiP path disabled (legacy behavior).
class DocumentPipStandaloneDisabledBrowserTest
    : public DocumentPipStandaloneBrowserTestBase {
 public:
  DocumentPipStandaloneDisabledBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kDocumentPictureInPictureAPI},
        /*disabled_features=*/{features::kDocumentPipStandaloneWindow});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// With the flag off, Document PiP still opens a Browser-backed window and no
// DocumentPipHost is created.
IN_PROC_BROWSER_TEST_F(DocumentPipStandaloneDisabledBrowserTest,
                       RequestWindowUsesBrowserBackedPath) {
  OpenDocumentPipWindow();

  auto* child_web_contents =
      PictureInPictureWindowManager::GetInstance()->GetChildWebContents();
  ASSERT_NE(nullptr, child_web_contents);

  EXPECT_EQ(nullptr, GetDocumentPipHost());
  EXPECT_EQ(1, CountPictureInPictureBrowsers());
}

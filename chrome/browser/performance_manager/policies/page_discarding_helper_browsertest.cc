// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace performance_manager::policies {

namespace {

class FaviconWatcher final : public content::WebContentsObserver {
 public:
  explicit FaviconWatcher(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~FaviconWatcher() final = default;

  FaviconWatcher(const FaviconWatcher&) = delete;
  FaviconWatcher& operator=(const FaviconWatcher&) = delete;

  void Wait() { run_loop_.Run(); }

 private:
  // WebContentsObserver
  void DidUpdateFaviconURL(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) final {
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
};

class PageDiscardingHelperBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Opens a new page in the background, and returns its index in the tab strip.
  int OpenNewBackgroundPage() {
    content::WindowedNotificationObserver load(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
    // Load a page with title and favicon so that some tests can manipulate
    // them.
    content::OpenURLParams page(
        embedded_test_server()->GetURL("/favicon/title2_with_favicon.html"),
        content::Referrer(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
        ui::PAGE_TRANSITION_TYPED, false);
    content::WebContents* contents = browser()->OpenURL(page);

    // Wait for the page and the initial favicon to finish loading.
    FaviconWatcher favicon_watcher(contents);
    load.Wait();
    favicon_watcher.Wait();

    return browser()->tab_strip_model()->GetIndexOfWebContents(contents);
  }

  void UpdatePageTitle(int index) {
    constexpr char16_t kNewTitle[] = u"New title";
    content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(index);
    content::TitleWatcher title_watcher(contents, kNewTitle);
    ASSERT_TRUE(content::ExecJs(
        contents, base::StrCat({"document.title = '",
                                base::UTF16ToASCII(kNewTitle), "'"})));
    EXPECT_EQ(title_watcher.WaitAndGetTitle(), kNewTitle);
  }

  void UpdateFavicon(int index) {
    content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(index);
    // Change the favicon link from "icon.png" to "icon.svg".
    FaviconWatcher favicon_watcher(contents);
    ASSERT_TRUE(content::ExecJs(
        contents,
        "document.getElementsByTagName('link')[0].href = 'icon.svg'"));
    favicon_watcher.Wait();
  }

  void ExpectImmediateDiscard(int index, bool expected_result) {
    base::WeakPtr<PageNode> page_node =
        PerformanceManager::GetPrimaryPageNodeForWebContents(
            browser()->tab_strip_model()->GetWebContentsAt(index));
    base::RunLoop run_loop;
    PerformanceManager::CallOnGraph(
        FROM_HERE, base::BindLambdaForTesting([&](Graph* graph) {
          ASSERT_TRUE(page_node);
          auto* helper = PageDiscardingHelper::GetFromGraph(graph);
          ASSERT_TRUE(helper);
          helper->ImmediatelyDiscardSpecificPage(
              page_node.get(), ::mojom::LifecycleUnitDiscardReason::URGENT,
              base::BindLambdaForTesting([&](bool success) {
                EXPECT_EQ(success, expected_result);
                run_loop.Quit();
              }));
        }));
    run_loop.Run();

    EXPECT_EQ(
        browser()->tab_strip_model()->GetWebContentsAt(index)->WasDiscarded(),
        expected_result);
  }
};

IN_PROC_BROWSER_TEST_F(PageDiscardingHelperBrowserTest, DiscardSpecificPage) {
  // Background pages can be discarded.
  const int index1 = OpenNewBackgroundPage();
  ExpectImmediateDiscard(index1, true);

  // Foreground page should be blocked.
  const int index2 = OpenNewBackgroundPage();
  browser()->tab_strip_model()->ActivateTabAt(index2);
  ExpectImmediateDiscard(index2, false);

  // Updating the title while in the background should block the discard.
  const int index3 = OpenNewBackgroundPage();
  UpdatePageTitle(index3);
  ExpectImmediateDiscard(index3, false);

  // Updating the page title while in the foreground should not.
  const int index4 = OpenNewBackgroundPage();
  browser()->tab_strip_model()->ActivateTabAt(index4);
  UpdatePageTitle(index4);
  browser()->tab_strip_model()->ActivateTabAt(index3);
  ExpectImmediateDiscard(index4, true);

  // Updating the favicon while in the background should block the discard.
  const int index5 = OpenNewBackgroundPage();
  UpdateFavicon(index5);
  ExpectImmediateDiscard(index5, false);

  // Updating the favicon while in the foreground should not.
  const int index6 = OpenNewBackgroundPage();
  browser()->tab_strip_model()->ActivateTabAt(index6);
  UpdateFavicon(index6);
  browser()->tab_strip_model()->ActivateTabAt(index5);
  ExpectImmediateDiscard(index6, true);
}

}  // namespace

}  // namespace performance_manager::policies

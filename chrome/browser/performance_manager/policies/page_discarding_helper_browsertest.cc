// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"

#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace performance_manager::policies {

namespace {

using DiscardReason = PageDiscardingHelper::DiscardReason;

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
    // Load a page with title and favicon so that some tests can manipulate
    // them.
    content::OpenURLParams page(
        embedded_test_server()->GetURL("/favicon/title2_with_favicon.html"),
        content::Referrer(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
        ui::PAGE_TRANSITION_TYPED, false);
    content::WebContents* contents =
        browser()->OpenURL(page, /*navigation_handle_callback=*/{});
    content::TestNavigationObserver observer(contents);
    observer.set_expected_initial_url(page.url);

    // Wait for the page and the initial favicon to finish loading.
    FaviconWatcher favicon_watcher(contents);
    observer.Wait();
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

  void ExpectImmediateDiscard(
      int index,
      DiscardReason discard_reason,
      bool expected_result,
      const base::Location& location = base::Location::Current()) {
    const char* discard_string;
    switch (discard_reason) {
      case DiscardReason::URGENT:
        discard_string = "Urgent";
        break;
      case DiscardReason::PROACTIVE:
        discard_string = "Proactive";
        break;
      case DiscardReason::EXTERNAL:
        discard_string = "External";
        break;
      case DiscardReason::SUGGESTED:
        discard_string = "Suggested";
        break;
    }
    SCOPED_TRACE(::testing::Message()
                 << discard_string << " discard from " << location.ToString());
    base::WeakPtr<PageNode> page_node =
        PerformanceManager::GetPrimaryPageNodeForWebContents(
            browser()->tab_strip_model()->GetWebContentsAt(index));
    base::RunLoop run_loop;
    PerformanceManager::CallOnGraph(
        FROM_HERE, base::BindLambdaForTesting([&](Graph* graph) {
          SCOPED_TRACE(::testing::Message()
                       << discard_string << " discard, called on graph from "
                       << location.ToString());
          ASSERT_TRUE(page_node);
          auto* helper = PageDiscardingHelper::GetFromGraph(graph);
          ASSERT_TRUE(helper);
          helper->ImmediatelyDiscardMultiplePages(
              {page_node.get()}, discard_reason,
              base::BindLambdaForTesting(
                  [&](std::optional<base::TimeTicks> first_discarded_at) {
                    EXPECT_EQ(first_discarded_at.has_value(), expected_result);
                    run_loop.Quit();
                  }));
        }));
    run_loop.Run();

    EXPECT_EQ(
        browser()->tab_strip_model()->GetWebContentsAt(index)->WasDiscarded(),
        expected_result);
  }
};

IN_PROC_BROWSER_TEST_F(PageDiscardingHelperBrowserTest,
                       DISABLED_DiscardSpecificPage) {
  // Test urgent and proactive discards in a loop to avoid the overhead of
  // starting a new browser every time.
  // TODO(crbug.com/40899366): Add tests for all the other heuristics in
  // PageDiscardingHelper::CanDiscard().
  for (auto discard_reason :
       {DiscardReason::URGENT, DiscardReason::PROACTIVE}) {
    {
      // Background pages can be discarded.
      const int index1 = OpenNewBackgroundPage();
      ExpectImmediateDiscard(index1, discard_reason, true);

      // Foreground page should be blocked.
      // TODO(crbug.com/40899366): Also test when the browser window is
      // occluded. They should still be blocked.
      const int index2 = OpenNewBackgroundPage();
      browser()->tab_strip_model()->ActivateTabAt(index2);
      ExpectImmediateDiscard(index2, discard_reason, false);
    }

    {
      // Updating the title while in the background should block only proactive
      // discards.
      const int index1 = OpenNewBackgroundPage();
      UpdatePageTitle(index1);
      ExpectImmediateDiscard(index1, discard_reason,
                             discard_reason == DiscardReason::URGENT);

      // Updating the page title while in the foreground should not block any
      // discards.
      const int index2 = OpenNewBackgroundPage();
      browser()->tab_strip_model()->ActivateTabAt(index2);
      UpdatePageTitle(index2);
      browser()->tab_strip_model()->ActivateTabAt(index1);
      ExpectImmediateDiscard(index2, discard_reason, true);
    }

    {
      // Updating the favicon while in the background should block only
      // proactive discards.
      const int index1 = OpenNewBackgroundPage();
      UpdateFavicon(index1);
      ExpectImmediateDiscard(index1, discard_reason,
                             discard_reason == DiscardReason::URGENT);

      // Updating the favicon while in the foreground should not block any
      // discards.
      const int index2 = OpenNewBackgroundPage();
      browser()->tab_strip_model()->ActivateTabAt(index2);
      UpdateFavicon(index2);
      browser()->tab_strip_model()->ActivateTabAt(index1);
      ExpectImmediateDiscard(index2, discard_reason, true);
    }
  }
}

}  // namespace

}  // namespace performance_manager::policies

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_cache.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_annotations_switches.h"
#include "components/page_content_annotations/core/page_content_cache_handler.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace page_content_annotations {

namespace {

class TestPageContentCacheObserver : public PageContentCache::Observer {
 public:
  explicit TestPageContentCacheObserver(PageContentCache* cache) {
    observation_.Observe(cache);
  }

  void OnCachePopulated(int64_t tab_id) override {
    populated_tabs_.insert(tab_id);
    if (waiting_populated_tab_id_ == tab_id && populated_quit_closure_) {
      std::move(populated_quit_closure_).Run();
    }
  }

  void OnCacheRemoved(int64_t tab_id) override {
    removed_tabs_.insert(tab_id);
    if (waiting_removed_tab_id_ == tab_id && removed_quit_closure_) {
      std::move(removed_quit_closure_).Run();
    }
  }

  void WaitForPopulated(int64_t tab_id) {
    if (populated_tabs_.contains(tab_id)) {
      return;
    }
    waiting_populated_tab_id_ = tab_id;
    base::RunLoop run_loop;
    populated_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    waiting_populated_tab_id_.reset();
  }

  void WaitForRemoved(int64_t tab_id) {
    if (removed_tabs_.contains(tab_id)) {
      return;
    }
    waiting_removed_tab_id_ = tab_id;
    base::RunLoop run_loop;
    removed_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    waiting_removed_tab_id_.reset();
  }

 private:
  base::ScopedObservation<PageContentCache, PageContentCache::Observer>
      observation_{this};

  std::set<int64_t> populated_tabs_;
  // Tab ID currently being waited on in `WaitForPopulated()`, if any.
  std::optional<int64_t> waiting_populated_tab_id_;
  base::OnceClosure populated_quit_closure_;

  std::set<int64_t> removed_tabs_;
  // Tab ID currently being waited on in `WaitForRemoved()`, if any.
  std::optional<int64_t> waiting_removed_tab_id_;
  base::OnceClosure removed_quit_closure_;
};

class TestExtractionObserver : public PageContentExtractionService::Observer {
 public:
  explicit TestExtractionObserver(PageContentExtractionService* service) {
    observation_.Observe(service);
  }

  void OnPageContentExtracted(content::Page& page,
                              PageContent page_content) override {
    content::WebContents* contents =
        content::WebContents::FromRenderFrameHost(&page.GetMainDocument());
    if (!contents) {
      return;
    }
    const int64_t tab_id = sessions::SessionTabHelper::IdForTab(contents).id();
    extracted_tab_ids_.insert(tab_id);
    if (waiting_tab_id_ == tab_id && quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  void WaitForExtraction(content::WebContents* contents) {
    const int64_t tab_id = sessions::SessionTabHelper::IdForTab(contents).id();
    if (extracted_tab_ids_.contains(tab_id)) {
      return;
    }
    waiting_tab_id_ = tab_id;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    waiting_tab_id_.reset();
  }

 private:
  base::ScopedObservation<PageContentExtractionService,
                          PageContentExtractionService::Observer>
      observation_{this};

  std::set<int64_t> extracted_tab_ids_;
  // Tab ID currently being waited on in `WaitForExtraction()`, if any.
  std::optional<int64_t> waiting_tab_id_;
  base::OnceClosure quit_closure_;
};

}  // namespace

class PageContentCacheBrowserTest : public InProcessBrowserTest {
 public:
  PageContentCacheBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kPageContentCache, {}},
         {features::kAnnotatedPageContentExtraction,
          {{"capture_delay", "0s"}, {"triggering_mode", "on_load"}}}},
        /*disabled_features=*/
        // TODO(b/579100952): Update
        // `kPageContentAnnotationsSkipFCPWaitForTesting` to also bypass
        // `PageSettledMonitor`'s visual-state and LCP wait in browser tests.
        {features::kPageContentExtractionUsingPageSettledMonitor});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kPageContentAnnotationsSkipFCPWaitForTesting);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  PageContentExtractionService* GetExtractionService() {
    return PageContentExtractionServiceFactory::GetForProfile(
        browser()->GetProfile());
  }

  PageContentCache* GetPageContentCache() {
    return GetExtractionService()
        ->page_content_cache_handler_->page_content_cache();
  }

  std::optional<optimization_guide::proto::PageContext>
  GetCachedPageContentForTab(int64_t tab_id) {
    base::test::TestFuture<
        std::optional<optimization_guide::proto::PageContext>>
        future;
    GetPageContentCache()->GetPageContentForTab(tab_id, future.GetCallback());
    return future.Take();
  }

  // Opens a new foreground tab at `index`. In headless/Xvfb Aura browser tests,
  // detaching the previous tab's view does not always trigger an
  // occlusion-driven `Visibility::HIDDEN` notification, so explicitly hide
  // `backgrounded_contents` if needed.
  void OpenForegroundTabAndHidePrevious(
      int index,
      const GURL& url,
      content::WebContents* backgrounded_contents) {
    EXPECT_TRUE(AddTabAtIndex(index, url, ui::PAGE_TRANSITION_TYPED));
    if (backgrounded_contents->GetVisibility() != content::Visibility::HIDDEN) {
      backgrounded_contents->WasHidden();
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageContentCacheBrowserTest,
                       CacheBehaviorOnTabSwitchAndClose) {
  ASSERT_TRUE(GetExtractionService());
  PageContentCache* cache = GetPageContentCache();
  ASSERT_TRUE(cache);
  TestPageContentCacheObserver cache_observer(cache);
  TestExtractionObserver extraction_observer(GetExtractionService());

  const GURL url1 = embedded_test_server()->GetURL(
      "a.test", "/optimization_guide/hello.html");
  content::WebContents* wc0 =
      browser()->GetTabStripModel()->GetWebContentsAt(0);
  const int64_t tab0_id = sessions::SessionTabHelper::IdForTab(wc0).id();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  extraction_observer.WaitForExtraction(wc0);

  // Open a second tab in the foreground, pushing tab 0 to the background.
  const GURL url2 = embedded_test_server()->GetURL(
      "b.test", "/optimization_guide/hello.html");
  OpenForegroundTabAndHidePrevious(1, url2, wc0);

  cache_observer.WaitForPopulated(tab0_id);
  std::optional<optimization_guide::proto::PageContext> cached =
      GetCachedPageContentForTab(tab0_id);
  ASSERT_TRUE(cached.has_value());
  EXPECT_EQ(cached->url(), url1.spec());

  // Close tab 0 and verify its cached entry is removed.
  base::HistogramTester histogram_tester;
  browser()->GetTabStripModel()->CloseWebContentsAt(0,
                                                    TabCloseTypes::CLOSE_NONE);

  cache_observer.WaitForRemoved(tab0_id);
  EXPECT_FALSE(GetCachedPageContentForTab(tab0_id).has_value());
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus",
      5 /* kContentsDeletedOnTabClose */, 1);
}

IN_PROC_BROWSER_TEST_F(PageContentCacheBrowserTest,
                       CloseActiveTabDoesNotSpuriouslyCache) {
  ASSERT_TRUE(GetExtractionService());
  PageContentCache* cache = GetPageContentCache();
  ASSERT_TRUE(cache);
  TestPageContentCacheObserver cache_observer(cache);
  TestExtractionObserver extraction_observer(GetExtractionService());

  // Open tab 1 in the foreground and wait for extraction while it remains the
  // active foreground tab.
  const GURL url = embedded_test_server()->GetURL(
      "a.test", "/optimization_guide/hello.html");
  ASSERT_TRUE(AddTabAtIndex(1, url, ui::PAGE_TRANSITION_TYPED));
  content::WebContents* wc1 =
      browser()->GetTabStripModel()->GetWebContentsAt(1);
  ASSERT_EQ(browser()->GetTabStripModel()->GetActiveWebContents(), wc1);
  wc1->UpdateWebContentsVisibility(content::Visibility::VISIBLE);
  const int64_t tab1_id = sessions::SessionTabHelper::IdForTab(wc1).id();

  extraction_observer.WaitForExtraction(wc1);

  // Closing the active foreground tab should not treat it as backgrounded or
  // cache its content.
  base::HistogramTester histogram_tester;
  browser()->GetTabStripModel()->CloseWebContentsAt(1,
                                                    TabCloseTypes::CLOSE_NONE);

  cache_observer.WaitForRemoved(tab1_id);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus",
      5 /* kContentsDeletedOnTabClose */, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus",
      3 /* kContentsAvailableWhenBackgrounded */, 0);
  EXPECT_FALSE(GetCachedPageContentForTab(tab1_id).has_value());
}

IN_PROC_BROWSER_TEST_F(PageContentCacheBrowserTest,
                       MoveTabBetweenWindowsAndCloseSecondaryWindow) {
  ASSERT_TRUE(GetExtractionService());
  PageContentCache* cache = GetPageContentCache();
  ASSERT_TRUE(cache);
  TestPageContentCacheObserver cache_observer(cache);
  TestExtractionObserver extraction_observer(GetExtractionService());

  const GURL url1 = embedded_test_server()->GetURL(
      "a.test", "/optimization_guide/hello.html");
  content::WebContents* wc0 =
      browser()->GetTabStripModel()->GetWebContentsAt(0);
  const int64_t tab0_id = sessions::SessionTabHelper::IdForTab(wc0).id();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  extraction_observer.WaitForExtraction(wc0);

  // Open tab 1 in foreground so tab 0 is backgrounded and cached.
  const GURL url2 = embedded_test_server()->GetURL(
      "b.test", "/optimization_guide/hello.html");
  OpenForegroundTabAndHidePrevious(1, url2, wc0);
  cache_observer.WaitForPopulated(tab0_id);
  ASSERT_TRUE(GetCachedPageContentForTab(tab0_id).has_value());

  // Create a second browser window for the same profile and move tab 0 into it
  // in the background.
  BrowserWindowInterface* browser2 = CreateBrowser(browser()->GetProfile());
  std::unique_ptr<tabs::TabModel> detached_tab =
      browser()->GetTabStripModel()->DetachTabAtForInsertion(0);
  browser2->GetTabStripModel()->InsertDetachedTabAt(1, std::move(detached_tab),
                                                    AddTabTypes::ADD_NONE);

  // Moving a tab between windows must not evict its cached content.
  EXPECT_TRUE(GetCachedPageContentForTab(tab0_id).has_value());

  // Closing the secondary window while the primary window stays open should
  // evict the tabs in the closed secondary window.
  CloseBrowserSynchronously(browser2);
  cache_observer.WaitForRemoved(tab0_id);
  EXPECT_FALSE(GetCachedPageContentForTab(tab0_id).has_value());
}

class PageContentCacheDiscardBrowserTest
    : public PageContentCacheBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  PageContentCacheDiscardBrowserTest() {
    web_contents_discard_feature_list_.InitWithFeatureState(
        ::features::kWebContentsDiscard, IsWebContentsDiscardEnabled());
  }

  bool IsWebContentsDiscardEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList web_contents_discard_feature_list_;
};

IN_PROC_BROWSER_TEST_P(PageContentCacheDiscardBrowserTest,
                       PreserveCacheOnTabDiscard) {
  ASSERT_TRUE(GetExtractionService());
  PageContentCache* cache = GetPageContentCache();
  ASSERT_TRUE(cache);
  TestPageContentCacheObserver cache_observer(cache);
  TestExtractionObserver extraction_observer(GetExtractionService());

  const GURL url1 = embedded_test_server()->GetURL(
      "a.test", "/optimization_guide/hello.html");
  content::WebContents* wc0 =
      browser()->GetTabStripModel()->GetWebContentsAt(0);
  const int64_t tab0_id = sessions::SessionTabHelper::IdForTab(wc0).id();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  extraction_observer.WaitForExtraction(wc0);

  // Open tab 1 in foreground so tab 0 is backgrounded and cached.
  const GURL url2 = embedded_test_server()->GetURL(
      "b.test", "/optimization_guide/hello.html");
  OpenForegroundTabAndHidePrevious(1, url2, wc0);
  cache_observer.WaitForPopulated(tab0_id);
  ASSERT_TRUE(GetCachedPageContentForTab(tab0_id).has_value());

  TabListInterface* tab_list = TabListInterface::From(browser());
  ASSERT_TRUE(tab_list);
  content::WebContents* new_wc0 =
      tab_list->DiscardTab(tab_list->GetTab(0)->GetHandle());
  ASSERT_TRUE(new_wc0);
  EXPECT_TRUE(new_wc0->WasDiscarded());
  const int64_t new_tab0_id =
      sessions::SessionTabHelper::IdForTab(new_wc0).id();
  if (IsWebContentsDiscardEnabled()) {
    // In-place discard preserves the existing `WebContents` and `SessionID`.
    EXPECT_EQ(new_wc0, wc0);
    EXPECT_EQ(new_tab0_id, tab0_id);
  } else {
    // Replacing the `WebContents` assigns a new `SessionID` and migrates the
    // cached entry.
    EXPECT_NE(new_wc0, wc0);
    ASSERT_NE(new_tab0_id, tab0_id);
    EXPECT_FALSE(GetCachedPageContentForTab(tab0_id).has_value());
  }

  std::optional<optimization_guide::proto::PageContext> cached =
      GetCachedPageContentForTab(new_tab0_id);
  ASSERT_TRUE(cached.has_value());
  EXPECT_EQ(cached->url(), url1.spec());

  // Close the discarded tab 0 and verify the entry is evicted.
  browser()->GetTabStripModel()->CloseWebContentsAt(0,
                                                    TabCloseTypes::CLOSE_NONE);
  cache_observer.WaitForRemoved(new_tab0_id);
  EXPECT_FALSE(GetCachedPageContentForTab(new_tab0_id).has_value());
}

INSTANTIATE_TEST_SUITE_P(All,
                         PageContentCacheDiscardBrowserTest,
                         testing::Bool());

}  // namespace page_content_annotations

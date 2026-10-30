// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_cache.h"

#include <stdint.h>

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
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_annotations_switches.h"
#include "components/page_content_annotations/core/page_content_cache_handler.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/page.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace page_content_annotations {

namespace {

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

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageContentCacheBrowserTest,
                       CloseActiveTabDoesNotSpuriouslyCache) {
  ASSERT_TRUE(GetExtractionService());
  ASSERT_TRUE(GetPageContentCache());
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

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus",
      3 /* kContentsAvailableWhenBackgrounded */, 0);
  EXPECT_FALSE(GetCachedPageContentForTab(tab1_id).has_value());
}

}  // namespace page_content_annotations

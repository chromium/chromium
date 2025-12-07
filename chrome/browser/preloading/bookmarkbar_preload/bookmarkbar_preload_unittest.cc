// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/timer/elapsed_timer.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/preloading/bookmarkbar_preload/bookmarkbar_preload_pipeline_manager.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/web_contents_tester.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BookmarkBarPreloadPipelineManagerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  BookmarkBarPreloadPipelineManagerTest(
      const BookmarkBarPreloadPipelineManagerTest&) = delete;
  BookmarkBarPreloadPipelineManagerTest& operator=(
      const BookmarkBarPreloadPipelineManagerTest&) = delete;
  BookmarkBarPreloadPipelineManagerTest()
      : ChromeRenderViewHostTestHarness(
            content::BrowserTaskEnvironment::REAL_IO_THREAD),
        prerender_helper_(base::BindRepeating(
            &BookmarkBarPreloadPipelineManagerTest::GetActiveWebContents,
            base::Unretained(this))) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kBookmarkTriggerForPrefetch);
  }

  void SetUp() override {
    ASSERT_TRUE(test_server_.Start());
    ChromeRenderViewHostTestHarness::SetUp();

    // Set up TemplateURLService for search prerendering.
    TemplateURLServiceFactoryTestUtil factory_util(profile());
    factory_util.VerifyLoad();

    TemplateURLData template_url_data;
    template_url_data.SetURL(GetUrl(search_site() + "?q={searchTerms}").spec());
    factory_util.model()->SetUserSelectedDefaultSearchProvider(
        factory_util.model()->Add(
            std::make_unique<TemplateURL>(template_url_data)));

    bookmarkbar_preload_manager_ =
        std::make_unique<BookmarkBarPreloadPipelineManager>(
            GetActiveWebContents());
  }

  content::WebContents* GetActiveWebContents() { return web_contents(); }

  GURL GetSearchSuggestionUrl(const std::string& original_query,
                              const std::string& search_terms) {
    return GetUrl(search_site() + "?q=" + search_terms +
                  "&oq=" + original_query);
  }

  void SimulateNavigationCommitted(const GURL& url) {
    auto* web_content_tester = content::WebContentsTester::For(web_contents());
    web_content_tester->SetLastCommittedURL(url);
    web_content_tester->NavigateAndCommit(url);
    web_content_tester->TestDidFinishLoad(url);

    // The main goal of `SimulateNavigationCommitted` is used to flush metrics
    // for tests, `RunUntilIdle` here allows metrics to have time to be flushed.
    base::RunLoop().RunUntilIdle();
  }

  ukm::TestAutoSetUkmRecorder& ukm_recorder() { return ukm_recorder_; }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

 protected:
  GURL GetUrl(const std::string& path) { return test_server_.GetURL(path); }

  BookmarkBarPreloadPipelineManager* bookmarkbar_preload_manager() {
    return bookmarkbar_preload_manager_.get();
  }

 private:
  static std::string search_site() { return "/title1.html"; }

  content::test::PrerenderTestHelper prerender_helper_;

  net::EmbeddedTestServer test_server_;

  base::test::ScopedFeatureList scoped_feature_list_;

  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  // This timer is for making TimeToNextNavigation in UKM consistent.
  base::ScopedMockElapsedTimersForTest test_timer_;

  std::unique_ptr<BookmarkBarPreloadPipelineManager>
      bookmarkbar_preload_manager_;
};

// Test that a search related url is ignored by the prefetch and prerender
// BookmarkBar trigger properly when both are triggered.
TEST_F(BookmarkBarPreloadPipelineManagerTest,
       PrefetchAndPrerenderDisallowSearchUrl) {
  base::HistogramTester histogram_tester;
  GURL preloading_url = GetSearchSuggestionUrl("prel", "preloading");
  std::unique_ptr<content::test::PreloadingAttemptUkmEntryBuilder>
      ukm_entry_builder =
          std::make_unique<content::test::PreloadingAttemptUkmEntryBuilder>(
              chrome_preloading_predictor::kMouseHoverOrMouseDownOnBookmarkBar);
  bookmarkbar_preload_manager()->StartPrefetch(preloading_url);
  bookmarkbar_preload_manager()->StartPrerender(preloading_url);
  EXPECT_FALSE(bookmarkbar_preload_manager()->IsPrerenderValidForTesting());

  histogram_tester.ExpectUniqueSample(
      "Prerender.IsPrerenderingSRPUrl.Embedder_BookmarkBar", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Navigation.Prefetch.IsPrefetchingSRPUrl.Embedder_BookmarkBar", true, 1);

  SimulateNavigationCommitted(preloading_url);
  {
    auto attempt_ukm_entries =
        ukm_recorder().GetEntries(ukm::builders::Preloading_Attempt::kEntryName,
                                  content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(attempt_ukm_entries.size(), 2u);

    ukm::SourceId ukm_source_id =
        GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
    content::test::ExpectPreloadingAttemptUkm(
        ukm_recorder(),
        {ukm_entry_builder->BuildEntry(
             ukm_source_id, content::PreloadingType::kPrefetch,
             static_cast<content::PreloadingEligibility>(
                 ChromePreloadingEligibility::KDisallowSearchUrl),
             content::PreloadingHoldbackStatus::kUnspecified,
             content::PreloadingTriggeringOutcome::kUnspecified,
             content::PreloadingFailureReason::kUnspecified,
             /*accurate=*/true),
         ukm_entry_builder->BuildEntry(
             ukm_source_id, content::PreloadingType::kPrerender,
             static_cast<content::PreloadingEligibility>(
                 ChromePreloadingEligibility::KDisallowSearchUrl),
             content::PreloadingHoldbackStatus::kUnspecified,
             content::PreloadingTriggeringOutcome::kUnspecified,
             content::PreloadingFailureReason::kUnspecified,
             /*accurate=*/true)});
  }
}

// Test that a search related url is ignored by the prefetch BookmarkBar
// trigger.
TEST_F(BookmarkBarPreloadPipelineManagerTest, PrefetchDisallowSearchUrl) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::test::PreloadingAttemptUkmEntryBuilder>
      ukm_entry_builder =
          std::make_unique<content::test::PreloadingAttemptUkmEntryBuilder>(
              chrome_preloading_predictor::kMouseHoverOrMouseDownOnBookmarkBar);
  GURL prefetch_url = GetSearchSuggestionUrl("pref", "prefetch");
  bookmarkbar_preload_manager()->StartPrefetch(prefetch_url);

  histogram_tester.ExpectUniqueSample(
      "Navigation.Prefetch.IsPrefetchingSRPUrl.Embedder_BookmarkBar", true, 1);

  SimulateNavigationCommitted(prefetch_url);
  {
    auto attempt_ukm_entries =
        ukm_recorder().GetEntries(ukm::builders::Preloading_Attempt::kEntryName,
                                  content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(attempt_ukm_entries.size(), 1u);

    ukm::SourceId ukm_source_id =
        GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
    content::test::ExpectPreloadingAttemptUkm(
        ukm_recorder(),
        {ukm_entry_builder->BuildEntry(
            ukm_source_id, content::PreloadingType::kPrefetch,
            static_cast<content::PreloadingEligibility>(
                ChromePreloadingEligibility::KDisallowSearchUrl),
            content::PreloadingHoldbackStatus::kUnspecified,
            content::PreloadingTriggeringOutcome::kUnspecified,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true)});
  }
}

// Test that a non default search related url from a common search provider is
// ignored by the prefetch BookmarkBar trigger.
TEST_F(BookmarkBarPreloadPipelineManagerTest,
       PrefetchDisallowSearchUrlOtherThanDefaultSearchProvider) {
  base::HistogramTester histogram_tester;
  GURL prefetch_url = GURL("https://www.google.co.jp/search?q=123");
  TemplateURLServiceFactoryTestUtil factory_util(profile());
  EXPECT_FALSE(
      factory_util.model()->IsSearchResultsPageFromDefaultSearchProvider(
          prefetch_url));
  bookmarkbar_preload_manager()->StartPrefetch(prefetch_url);

  histogram_tester.ExpectUniqueSample(
      "Navigation.Prefetch.IsPrefetchingSRPUrl.Embedder_BookmarkBar", true, 1);
}

}  // namespace

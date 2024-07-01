// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/history_embeddings_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "components/page_content_annotations/core/test_page_content_annotator.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace history_embeddings {

class HistoryEmbeddingsBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    // The feature must be enabled first or else the service isn't initialized
    // properly.
    feature_list_.InitWithFeaturesAndParameters(
        {{kHistoryEmbeddings,
          {{"UseMlEmbedder", "false"}, {"SendQualityLog", "true"}}},
         {page_content_annotations::features::kPageContentAnnotations, {{}}},
#if BUILDFLAG(IS_CHROMEOS)
         {chromeos::features::kFeatureManagementHistoryEmbedding, {{}}}
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        /*disabled_features=*/{});

    InProcessBrowserTest::SetUp();
  }

 protected:
  HistoryEmbeddingsService* service() {
    return HistoryEmbeddingsServiceFactory::GetForProfile(browser()->profile());
  }

  page_content_annotations::PageContentAnnotationsService*
  page_content_annotations_service() {
    return PageContentAnnotationsServiceFactory::GetForProfile(
        browser()->profile());
  }

  base::RepeatingCallback<void(UrlPassages)>& callback_for_tests() {
    return service()->callback_for_tests_;
  }

  void OverrideVisibilityScoresForTesting(
      const base::flat_map<std::string, double>& visibility_scores_for_input) {
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        optimization_guide::TestModelInfoBuilder()
            .SetModelFilePath(
                base::FilePath(FILE_PATH_LITERAL("visibility_model")))
            .SetVersion(123)
            .Build();
    CHECK(model_info);
    page_content_annotator_.UseVisibilityScores(*model_info,
                                                visibility_scores_for_input);
    page_content_annotations_service()->OverridePageContentAnnotatorForTesting(
        &page_content_annotator_);
  }

  virtual void InitializeFeatureList() {}

  base::test::ScopedFeatureList feature_list_;

 private:
  page_content_annotations::TestPageContentAnnotator page_content_annotator_;
};

IN_PROC_BROWSER_TEST_F(HistoryEmbeddingsBrowserTest, ServiceFactoryWorks) {
  auto* service =
      HistoryEmbeddingsServiceFactory::GetForProfile(browser()->profile());
  EXPECT_TRUE(service);
}

IN_PROC_BROWSER_TEST_F(HistoryEmbeddingsBrowserTest, BrowserRetrievesPassages) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/inner_text/test1.html")));

  base::test::TestFuture<UrlPassages> future;
  callback_for_tests() = future.GetRepeatingCallback();
  service()->RetrievePassages(
      {}, web_contents->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  UrlPassages url_passages = future.Take();

  ASSERT_EQ(url_passages.passages.passages_size(), 1);
  ASSERT_EQ(url_passages.passages.passages(0), "A a B C b a 2 D");
}

IN_PROC_BROWSER_TEST_F(HistoryEmbeddingsBrowserTest,
                       SearchFindsResultWithSourcePassage) {
  OverrideVisibilityScoresForTesting({
      {"A a B C b a 2 D", 0.99},
  });

  ASSERT_TRUE(embedded_test_server()->Start());
  base::test::TestFuture<UrlPassages> store_future;
  callback_for_tests() = store_future.GetRepeatingCallback();

  const GURL url = embedded_test_server()->GetURL("/inner_text/test1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(store_future.Wait());

  base::HistogramTester histogram_tester;

  // Search for the passage.
  base::test::TestFuture<SearchResult> search_future;
  service()->Search("A B C D e f g", {}, 1, search_future.GetCallback());
  SearchResult result = search_future.Take();
  EXPECT_EQ(result.scored_url_rows.size(), 1u);
  EXPECT_EQ(result.scored_url_rows[0].scored_url.passage, "A a B C b a 2 D");
  EXPECT_EQ(result.scored_url_rows[0].row.url(), url);

  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.QueryEmbeddingSucceeded", true, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.VisibilityModelAvailableAtQuery", true, 1);
  histogram_tester.ExpectUniqueSample("History.Embeddings.NumUrlsMatched", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.NumMatchedUrlsVisible", 1, 1);
}

IN_PROC_BROWSER_TEST_F(
    HistoryEmbeddingsBrowserTest,
    SearchFiltersOutResultWithSourcePassageThatShouldNotBeVisible) {
  OverrideVisibilityScoresForTesting({{"A B C D", 0.14}});

  ASSERT_TRUE(embedded_test_server()->Start());
  base::test::TestFuture<UrlPassages> store_future;
  callback_for_tests() = store_future.GetRepeatingCallback();

  const GURL url = embedded_test_server()->GetURL("/inner_text/test1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(store_future.Wait());

  base::HistogramTester histogram_tester;

  // Search for the passage.
  base::test::TestFuture<SearchResult> search_future;
  service()->Search("A B C D e f g", {}, 1, search_future.GetCallback());
  SearchResult result = search_future.Take();
  EXPECT_TRUE(result.scored_url_rows.empty());

  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.QueryEmbeddingSucceeded", true, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.VisibilityModelAvailableAtQuery", true, 1);
  histogram_tester.ExpectUniqueSample("History.Embeddings.NumUrlsMatched", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.NumMatchedUrlsVisible", 0, 1);
}

IN_PROC_BROWSER_TEST_F(HistoryEmbeddingsBrowserTest,
                       SearchReturnsNoResultsVisibilityModelNotAvailable) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::test::TestFuture<UrlPassages> store_future;
  callback_for_tests() = store_future.GetRepeatingCallback();

  const GURL url = embedded_test_server()->GetURL("/inner_text/test1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(store_future.Wait());

  base::HistogramTester histogram_tester;

  // Search for the passage.
  base::test::TestFuture<SearchResult> search_future;
  service()->Search("A B C D e f g", {}, 1, search_future.GetCallback());
  SearchResult result = search_future.Take();
  EXPECT_TRUE(result.scored_url_rows.empty());

  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.QueryEmbeddingSucceeded", true, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.VisibilityModelAvailableAtQuery", false, 1);
  histogram_tester.ExpectUniqueSample("History.Embeddings.NumUrlsMatched", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.NumMatchedUrlsVisible", 0, 1);
}

IN_PROC_BROWSER_TEST_F(HistoryEmbeddingsBrowserTest, LogDataIsPrepared) {
  base::HistogramTester histogram_tester;
  SearchResult result;
  result.scored_url_rows = {
      ScoredUrlRow(ScoredUrl(0, 0, base::Time::Now(), 0.5f, 0, {})),
  };
  service()->SendQualityLog(result, 1, 3, false);
  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.Quality.LogEntryPrepared", true, 1);
}

}  // namespace history_embeddings

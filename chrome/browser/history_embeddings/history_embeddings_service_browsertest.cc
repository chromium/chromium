// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/content/history_embeddings_service.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_tab_helper.h"
#include "chrome/browser/history_embeddings/history_embeddings_utils.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/page_content_annotations/page_embeddings_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history_embeddings/core/history_embeddings_features.h"
#include "components/history_embeddings/core/mock_answerer.h"
#include "components/history_embeddings/core/mock_intent_classifier.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "components/page_content_annotations/core/test_page_content_annotator.h"
#include "components/passage_embeddings/core/passage_embeddings_test_util.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace history_embeddings {

class HistoryEmbeddingsBrowserTest : public InProcessBrowserTest {
 public:
  HistoryEmbeddingsBrowserTest() {
    dependency_manager_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &HistoryEmbeddingsBrowserTest::RegisterTestingServiceFactories,
                base::Unretained(this)));
  }

  void SetUp() override {
    InitializeFeatureList();
    InProcessBrowserTest::SetUp();
  }

  void RegisterTestingServiceFactories(content::BrowserContext* context) {
    HistoryEmbeddingsServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindLambdaForTesting([this](content::BrowserContext* context) {
          return HistoryEmbeddingsServiceFactory::
              BuildServiceInstanceForBrowserContextForTesting(
                  context,
                  passage_embeddings_test_env_.embedder_metadata_provider(),
                  passage_embeddings_test_env_.embedder(),
                  std::make_unique<MockAnswerer>(),
                  std::make_unique<MockIntentClassifier>());
        }));

    const auto generate_embeddings_candidates =
        [this](const page_content_annotations::PageContent&,
               size_t page_content_passages_to_generate) {
          std::vector<std::pair<std::string,
                                page_content_annotations::EmbeddingPassageType>>
              result;
          for (const std::string& passage : page_passages_) {
            result.emplace_back(
                passage,
                page_content_annotations::EmbeddingPassageType::kPageContent);
          }
          return result;
        };

    page_content_annotations::PageEmbeddingsServiceFactory::GetInstance()
        ->SetTestingFactory(
            context,
            base::BindLambdaForTesting([this, generate_embeddings_candidates](
                                           content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  page_content_annotations::PageEmbeddingsService>(
                  base::BindLambdaForTesting(generate_embeddings_candidates),
                  page_content_annotations::
                      PageContentExtractionServiceFactory::GetForProfile(
                          Profile::FromBrowserContext(context)),
                  passage_embeddings_test_env_.embedder(),
                  passage_embeddings_test_env_.embedder_metadata_provider());
            }));
  }

  void SetUpOnMainThread() override {
    InitSignin();
    browser()->profile()->GetPrefs()->SetInteger(
        optimization_guide::prefs::GetSettingEnabledPrefName(
            optimization_guide::UserVisibleFeatureKey::kHistorySearch),
        static_cast<int>(
            optimization_guide::prefs::FeatureOptInState::kEnabled));

    InProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  HistoryEmbeddingsTabHelper* tab_helper() {
    return HistoryEmbeddingsTabHelper::FromWebContents(GetActiveWebContents());
  }

  virtual void InitSignin() {
    OptimizationGuideKeyedService* optimization_guide_keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    optimization_guide_keyed_service->AllowUnsignedUserForTesting(
        optimization_guide::UserVisibleFeatureKey::kHistorySearch);
    optimization_guide::EnableSigninAndModelExecutionCapability(
        browser()->profile());
  }

  HistoryEmbeddingsService* service() {
    return HistoryEmbeddingsServiceFactory::GetForProfile(browser()->profile());
  }

  page_content_annotations::PageContentAnnotationsService*
  page_content_annotations_service() {
    return PageContentAnnotationsServiceFactory::GetForProfile(
        browser()->profile());
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

  GURL GetUrl() { return embedded_test_server()->GetURL("/links.html"); }

  // Triggers a page load, to cause the machinery behind generating page
  // embeddings to start working. The particular page to load doesn't matter
  // since we're overriding the passages to be used for the embeddings below, as
  // long as the page is sufficiently complex to trigger annotated page content
  // extraction.
  bool TriggerPageLoad() {
    return ui_test_utils::NavigateToURL(browser(), GetUrl());
  }

  virtual void InitializeFeatureList() {
    // The feature must be enabled first or else the service isn't initialized
    // properly.
    feature_list_.InitWithFeaturesAndParameters(
        {
            {kHistoryEmbeddings,
             {{"SendQualityLog", "true"},
              {"ContentVisibilityThreshold", "0.01"},
              {"UseUrlFilter", "false"}}},
            {kHistoryEmbeddingsAnswers, {{}}},
            {page_content_annotations::features::kPageContentAnnotations, {{}}},
#if BUILDFLAG(IS_CHROMEOS)
            {chromeos::features::kFeatureManagementHistoryEmbedding, {{}}},
            {chromeos::features::kFeatureManagementPassageEmbedder, {{}}},
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        /*disabled_features=*/{});
  }

  void SetPagePassages(std::vector<std::string> passages) {
    page_passages_ = std::move(passages);
  }

  base::test::ScopedFeatureList feature_list_;

 private:
  base::CallbackListSubscription dependency_manager_subscription_;
  page_content_annotations::TestPageContentAnnotator page_content_annotator_;
  passage_embeddings::TestEnvironment passage_embeddings_test_env_;
  std::vector<std::string> page_passages_{"A a B C b a 2 D"};
};

class HistoryEmbeddingsRestrictedSigninBrowserTest
    : public HistoryEmbeddingsBrowserTest {
 protected:
  void InitSignin() override {
    OptimizationGuideKeyedService* optimization_guide_keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    optimization_guide_keyed_service->AllowUnsignedUserForTesting(
        optimization_guide::UserVisibleFeatureKey::kHistorySearch);
    optimization_guide::EnableSigninWithoutModelExecutionCapability(
        browser()->profile());
  }
};

IN_PROC_BROWSER_TEST_F(HistoryEmbeddingsBrowserTest, ServiceFactoryWorks) {
  EXPECT_TRUE(service());
}

IN_PROC_BROWSER_TEST_F(HistoryEmbeddingsBrowserTest,
                       SearchFindsResultWithSourcePassage) {
  OverrideVisibilityScoresForTesting({
      {"A a B C b a 2 D", 0.99},
  });

  ASSERT_TRUE(embedded_test_server()->Start());
  base::test::TestFuture<UrlData> store_future;
  service()->SetPassagesStoredCallbackForTesting(
      store_future.GetRepeatingCallback());

  ASSERT_TRUE(TriggerPageLoad());
  EXPECT_TRUE(store_future.Wait());

  base::HistogramTester histogram_tester;

  // Search for the passage.
  base::test::TestFuture<SearchResult> search_future;
  service()->Search(nullptr, "A B C D e f g", {}, 1, /*skip_answering=*/false,
                    search_future.GetRepeatingCallback());
  SearchResult result = search_future.Take();
  ASSERT_EQ(result.scored_url_rows.size(), 1u);
  EXPECT_EQ(result.scored_url_rows[0].GetBestPassage(), "A a B C b a 2 D");
  EXPECT_EQ(result.scored_url_rows[0].row.url(), GetUrl());

  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.QueryEmbeddingSucceeded", true, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.VisibilityModelAvailableAtQuery", true, 1);
  histogram_tester.ExpectUniqueSample("History.Embeddings.NumUrlsMatched", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.NumMatchedUrlsVisible", 1, 1);
}

class HistoryEmbeddingsWithLowAggregationBrowserTest
    : public HistoryEmbeddingsBrowserTest {
  void InitializeFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{kHistoryEmbeddings,
          {{"SendQualityLog", "true"},
           {"PassageExtractionMaxWordsPerAggregatePassage", "10"}}},
#if BUILDFLAG(IS_CHROMEOS)
         {chromeos::features::kFeatureManagementHistoryEmbedding, {{}}},
         {chromeos::features::kFeatureManagementPassageEmbedder, {{}}},
#endif  // BUILDFLAG(IS_CHROMEOS)
         {page_content_annotations::features::kPageContentAnnotations, {{}}}},
        /*disabled_features=*/{});
  }
};

IN_PROC_BROWSER_TEST_F(HistoryEmbeddingsWithLowAggregationBrowserTest,
                       PassageContextSelectionLimitedByWordCount) {
  SetPagePassages({
      "Paragraph one with link and more. Header one Paragraph two.",
      "Paragraph three with link and more.",
      "Header two Paragraph four that puts entire div over length.",
      "Paragraph five with link and more. Paragraph six.",
      "Paragraph seven that puts entire div over length.",
  });
  OverrideVisibilityScoresForTesting({
      {"Paragraph one with link and more. Header one Paragraph two.", 0.99},
      {"Paragraph three with link and more.", 0.99},
      {"Header two Paragraph four that puts entire div over length.", 0.99},
      {"Paragraph five with link and more. Paragraph six.", 0.99},
      {"Paragraph seven that puts entire div over length.", 0.99},
  });

  ASSERT_TRUE(embedded_test_server()->Start());
  base::test::TestFuture<UrlData> store_future;
  service()->SetPassagesStoredCallbackForTesting(
      store_future.GetRepeatingCallback());

  // This HTML has <br> tags to separate the passages with known word counts.
  ASSERT_TRUE(TriggerPageLoad());
  EXPECT_TRUE(store_future.Wait());

  // Search for the passage.
  base::test::TestFuture<SearchResult> search_future;
  service()->Search(nullptr, "A B C", {}, 1, /*skip_answering=*/false,
                    search_future.GetRepeatingCallback());
  SearchResult result = search_future.Take();
  ASSERT_EQ(result.scored_url_rows.size(), 1u);
  EXPECT_EQ(result.scored_url_rows[0].GetBestPassage(),
            "Header two Paragraph four that puts entire div over length.");
  EXPECT_EQ(result.scored_url_rows[0].row.url(), GetUrl());

  // Can't exceed available passage count.
  EXPECT_EQ(result.scored_url_rows[0].GetBestScoreIndices(0, 0).size(), 0u);
  EXPECT_EQ(result.scored_url_rows[0].GetBestScoreIndices(1, 0).size(), 1u);
  EXPECT_EQ(result.scored_url_rows[0].GetBestScoreIndices(2, 0).size(), 2u);
  EXPECT_EQ(result.scored_url_rows[0].GetBestScoreIndices(3, 0).size(), 3u);
  EXPECT_EQ(result.scored_url_rows[0].GetBestScoreIndices(4, 0).size(), 4u);
  EXPECT_EQ(result.scored_url_rows[0].GetBestScoreIndices(5, 0).size(), 5u);
  EXPECT_EQ(result.scored_url_rows[0].GetBestScoreIndices(6, 0).size(), 5u);

  // Check that increasing word count grows context to include more passages.
  // Note that since scores are the same, higher word counts come first.
  // For the known passages, we have these counts, in order: 10, 10, 8, 8, 6.
  EXPECT_EQ(result.scored_url_rows[0].GetBestScoreIndices(0, 1).size(), 1u);
  EXPECT_EQ(result.scored_url_rows[0].GetBestScoreIndices(0, 10).size(), 1u);
  EXPECT_EQ(result.scored_url_rows[0].GetBestScoreIndices(0, 11).size(), 2u);
  EXPECT_EQ(result.scored_url_rows[0].GetBestScoreIndices(0, 20).size(), 2u);
  EXPECT_EQ(result.scored_url_rows[0].GetBestScoreIndices(0, 21).size(), 3u);
  EXPECT_EQ(result.scored_url_rows[0].GetBestScoreIndices(0, 28).size(), 3u);
  EXPECT_EQ(result.scored_url_rows[0].GetBestScoreIndices(0, 29).size(), 4u);
  EXPECT_EQ(result.scored_url_rows[0].GetBestScoreIndices(0, 36).size(), 4u);
  EXPECT_EQ(result.scored_url_rows[0].GetBestScoreIndices(0, 37).size(), 5u);
  EXPECT_EQ(result.scored_url_rows[0].GetBestScoreIndices(0, 1000).size(), 5u);
}

IN_PROC_BROWSER_TEST_F(
    HistoryEmbeddingsBrowserTest,
    SearchFiltersOutResultWithSourcePassageThatShouldNotBeVisible) {
  OverrideVisibilityScoresForTesting({{"A B C D", 0.14}});
  OverrideVisibilityScoresForTesting({{"A B C D e f g", 0.00}});

  ASSERT_TRUE(embedded_test_server()->Start());
  base::test::TestFuture<UrlData> store_future;
  service()->SetPassagesStoredCallbackForTesting(
      store_future.GetRepeatingCallback());

  ASSERT_TRUE(TriggerPageLoad());
  EXPECT_TRUE(store_future.Wait());

  base::HistogramTester histogram_tester;

  // Search for the passage.
  base::test::TestFuture<SearchResult> search_future;
  service()->Search(nullptr, "A B C D e f g", {}, 1, /*skip_answering=*/false,
                    search_future.GetRepeatingCallback());
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
  base::test::TestFuture<UrlData> store_future;
  service()->SetPassagesStoredCallbackForTesting(
      store_future.GetRepeatingCallback());

  ASSERT_TRUE(TriggerPageLoad());
  EXPECT_TRUE(store_future.Wait());

  base::HistogramTester histogram_tester;

  // Search for the passage.
  base::test::TestFuture<SearchResult> search_future;
  service()->Search(nullptr, "A B C D e f g", {}, 1, /*skip_answering=*/false,
                    search_future.GetRepeatingCallback());
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
      ScoredUrlRow(ScoredUrl(0, 0, base::Time::Now(), 0.5f, 0.2f)),
  };
  service()->SendQualityLog(
      result, {1}, 3,
      optimization_guide::proto::UserFeedback::USER_FEEDBACK_UNSPECIFIED,
      optimization_guide::proto::UiSurface::UI_SURFACE_HISTORY_PAGE);
  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.Quality.LogEntryPrepared", true, 1);
}

class HistoryEmbeddingsWithUrlFilterBrowserTest
    : public HistoryEmbeddingsBrowserTest {
  void InitializeFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{kHistoryEmbeddings,
          {{"SendQualityLog", "true"}, {"UseUrlFilter", "true"}}},
#if BUILDFLAG(IS_CHROMEOS)
         {chromeos::features::kFeatureManagementHistoryEmbedding, {{}}},
         {chromeos::features::kFeatureManagementPassageEmbedder, {{}}},
#endif  // BUILDFLAG(IS_CHROMEOS)
         {page_content_annotations::features::kPageContentAnnotations, {{}}}},
        /*disabled_features=*/{});
  }
};

IN_PROC_BROWSER_TEST_F(HistoryEmbeddingsWithUrlFilterBrowserTest,
                       FilterUrlOnBlocklist) {
  ASSERT_TRUE(embedded_test_server()->Start());
  OverrideVisibilityScoresForTesting({
      {"A a B C b a 2 D", 0.99},
  });

  base::test::TestFuture<UrlData> store_future;
  service()->SetPassagesStoredCallbackForTesting(
      store_future.GetRepeatingCallback());

  ASSERT_TRUE(TriggerPageLoad());
  EXPECT_TRUE(store_future.Wait());

  base::HistogramTester histogram_tester;

  // Search for the passage, should return empty result because of the filter.
  base::test::TestFuture<SearchResult> search_future;
  service()->Search(nullptr, "A B C D e f g", {}, 1, /*skip_answering=*/false,
                    search_future.GetRepeatingCallback());
  SearchResult result = search_future.Take();
  EXPECT_TRUE(result.scored_url_rows.empty());

  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.QueryEmbeddingSucceeded", true, 1);
  histogram_tester.ExpectUniqueSample("History.Embeddings.NumUrlsMatched", 0,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.NumMatchedUrlsVisible", 0, 1);
}

IN_PROC_BROWSER_TEST_F(HistoryEmbeddingsWithUrlFilterBrowserTest,
                       DoesNotFilterUrlNotOnBlocklist) {
  ASSERT_TRUE(embedded_test_server()->Start());
  OverrideVisibilityScoresForTesting({
      {"A a B C b a 2 D", 0.99},
  });
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(GetUrl(),
                          optimization_guide::proto::HISTORY_EMBEDDINGS,
                          std::nullopt);

  base::test::TestFuture<UrlData> store_future;
  service()->SetPassagesStoredCallbackForTesting(
      store_future.GetRepeatingCallback());

  ASSERT_TRUE(TriggerPageLoad());
  EXPECT_TRUE(store_future.Wait());

  base::HistogramTester histogram_tester;

  // Search for the passage; should have valid result since the URL is allowed.
  base::test::TestFuture<SearchResult> search_future;
  service()->Search(nullptr, "A B C D e f g", {}, 1, /*skip_answering=*/false,
                    search_future.GetRepeatingCallback());
  SearchResult result = search_future.Take();
  ASSERT_EQ(result.scored_url_rows.size(), 1u);
  EXPECT_EQ(result.scored_url_rows[0].GetBestPassage(), "A a B C b a 2 D");
  EXPECT_EQ(result.scored_url_rows[0].row.url(), GetUrl());

  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.QueryEmbeddingSucceeded", true, 1);
  histogram_tester.ExpectUniqueSample("History.Embeddings.NumUrlsMatched", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.NumMatchedUrlsVisible", 1, 1);
}

IN_PROC_BROWSER_TEST_F(HistoryEmbeddingsBrowserTest,
                       SearchReceivesAnswerWhenQueryIsAnswerable) {
  OverrideVisibilityScoresForTesting({
      {"A a B C b a 2 D", 0.99},
  });

  ASSERT_TRUE(embedded_test_server()->Start());
  base::test::TestFuture<UrlData> store_future;
  service()->SetPassagesStoredCallbackForTesting(
      store_future.GetRepeatingCallback());

  ASSERT_TRUE(TriggerPageLoad());
  EXPECT_TRUE(store_future.Wait());

  {
    base::HistogramTester histogram_tester;

    // Search with an answerable query by ending it with '?'.
    base::test::TestFuture<SearchResult> search_future;
    service()->Search(nullptr, "A B C D?", {}, 1, /*skip_answering=*/false,
                      search_future.GetRepeatingCallback());
    SearchResult first_result = search_future.Take();
    EXPECT_EQ(first_result.scored_url_rows.size(), 1u);
    EXPECT_EQ(first_result.answerer_result.status,
              ComputeAnswerStatus::kUnspecified);
    EXPECT_TRUE(first_result.AnswerText().empty());

    // Second published search result includes loading state.
    SearchResult second_result = search_future.Take();
    EXPECT_EQ(second_result.scored_url_rows.size(), 1u);
    EXPECT_EQ(second_result.answerer_result.status,
              ComputeAnswerStatus::kLoading);
    EXPECT_TRUE(second_result.AnswerText().empty());

    SearchResult final_result = search_future.Take();
    EXPECT_EQ(final_result.scored_url_rows.size(), 1u);
    EXPECT_EQ(final_result.answerer_result.status,
              ComputeAnswerStatus::kSuccess);
    EXPECT_FALSE(final_result.AnswerText().empty());

    histogram_tester.ExpectUniqueSample("History.Embeddings.QueryAnswerable",
                                        true, 1);
  }
  {
    base::HistogramTester histogram_tester;

    // Search with a query that does not signal query intent (not answerable).
    base::test::TestFuture<SearchResult> search_future;
    service()->Search(nullptr, "A B C D", {}, 1, /*skip_answering=*/false,
                      search_future.GetRepeatingCallback());
    SearchResult first_result = search_future.Take();
    EXPECT_EQ(first_result.scored_url_rows.size(), 1u);
    EXPECT_TRUE(first_result.AnswerText().empty());

    // Second search result with answer will never be published, and the
    // histogram being logged indicates the service finished without consulting
    // the answerer.
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return histogram_tester.GetBucketCount(
                 "History.Embeddings.QueryAnswerable", false) > 0;
    }));
    histogram_tester.ExpectUniqueSample("History.Embeddings.QueryAnswerable",
                                        false, 1);
  }
}

IN_PROC_BROWSER_TEST_F(HistoryEmbeddingsRestrictedSigninBrowserTest,
                       SearchDoesNotReceiveAnswerForRestrictedSignin) {
  OverrideVisibilityScoresForTesting({
      {"A a B C b a 2 D", 0.99},
  });

  ASSERT_TRUE(embedded_test_server()->Start());
  base::test::TestFuture<UrlData> store_future;
  service()->SetPassagesStoredCallbackForTesting(
      store_future.GetRepeatingCallback());

  ASSERT_TRUE(TriggerPageLoad());
  EXPECT_TRUE(store_future.Wait());

  base::HistogramTester histogram_tester;

  // Search with a query that signals question intent, but is not answerable
  // due to account restriction.
  base::test::TestFuture<SearchResult> search_future;
  service()->Search(nullptr, "A B C D?", {}, 1, /*skip_answering=*/false,
                    search_future.GetRepeatingCallback());
  SearchResult first_result = search_future.Take();
  EXPECT_EQ(first_result.scored_url_rows.size(), 1u);
  EXPECT_TRUE(first_result.AnswerText().empty());

  // Second search result with answer will never be published, and the
  // histogram being logged indicates the service finished without consulting
  // the answerer.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount("History.Embeddings.QueryAnswerable",
                                           false) > 0;
  }));
  histogram_tester.ExpectUniqueSample("History.Embeddings.QueryAnswerable",
                                      false, 1);
}

class HistoryEmbeddingsKillSwitchBrowserTest
    : public HistoryEmbeddingsBrowserTest {
  void InitializeFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {kHistoryEmbeddings,
             {{"SendQualityLog", "true"},
              {"ContentVisibilityThreshold", "0.01"},
              {"UseUrlFilter", "false"}}},
            {page_content_annotations::features::kPageContentAnnotations, {{}}},
#if BUILDFLAG(IS_CHROMEOS)
            {chromeos::features::kFeatureManagementHistoryEmbedding, {{}}},
            {chromeos::features::kFeatureManagementPassageEmbedder, {{}}},
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        /*disabled_features=*/{kLaunchedHistoryEmbeddings});
  }
};

IN_PROC_BROWSER_TEST_F(HistoryEmbeddingsKillSwitchBrowserTest,
                       NoCrashAfterKillSwitch) {
  OverrideVisibilityScoresForTesting({
      {"A a B C b a 2 D", 0.99},
  });

  ASSERT_TRUE(embedded_test_server()->Start());
  base::test::TestFuture<UrlData> store_future;
  service()->SetPassagesStoredCallbackForTesting(
      store_future.GetRepeatingCallback());

  ASSERT_TRUE(TriggerPageLoad());
  EXPECT_TRUE(store_future.Wait());

  {
    base::test::TestFuture<SearchResult> search_future;
    service()->Search(nullptr, "A B C D e f g", {}, 1, /*skip_answering=*/false,
                      search_future.GetRepeatingCallback());
    SearchResult result = search_future.Take();
    ASSERT_EQ(result.scored_url_rows.size(), 1u);
    EXPECT_EQ(result.scored_url_rows[0].GetBestPassage(), "A a B C b a 2 D");
    EXPECT_EQ(result.scored_url_rows[0].row.url(), GetUrl());
  }
}

}  // namespace history_embeddings

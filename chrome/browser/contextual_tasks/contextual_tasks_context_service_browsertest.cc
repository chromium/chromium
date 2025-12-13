// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service_factory.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_types.h"
#include "chrome/browser/passage_embeddings/page_embeddings_service.h"
#include "chrome/browser/passage_embeddings/page_embeddings_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_tasks/public/features.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "components/page_content_annotations/core/test_page_content_annotator.h"
#include "components/passage_embeddings/passage_embeddings_features.h"
#include "components/passage_embeddings/passage_embeddings_test_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace contextual_tasks {

using ::testing::_;
using ::testing::Return;

class FakeEmbedderMetadataProvider
    : public passage_embeddings::EmbedderMetadataProvider {
 public:
  FakeEmbedderMetadataProvider() = default;
  ~FakeEmbedderMetadataProvider() override = default;

  // passage_embeddings::EmbedderMetadataProvider:
  void AddObserver(
      passage_embeddings::EmbedderMetadataObserver* observer) override {
    observer_list_.AddObserver(observer);
  }
  void RemoveObserver(
      passage_embeddings::EmbedderMetadataObserver* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  void NotifyObservers() {
    observer_list_.Notify(
        &passage_embeddings::EmbedderMetadataObserver::EmbedderMetadataUpdated,
        passage_embeddings::EmbedderMetadata(1, 768));
  }

 private:
  base::ObserverList<passage_embeddings::EmbedderMetadataObserver>
      observer_list_;
};

class FakeEmbedder : public passage_embeddings::TestEmbedder {
 public:
  FakeEmbedder() = default;
  ~FakeEmbedder() override = default;

  // passage_embeddings::TestEmbedder:
  passage_embeddings::Embedder::TaskId ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority priority,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) override {
    if (status_ == passage_embeddings::ComputeEmbeddingsStatus::kSuccess) {
      passage_embeddings::TestEmbedder::ComputePassagesEmbeddings(
          priority, passages, std::move(callback));
      return 0;
    }

    std::move(callback).Run(passages, {}, 0, status_);
    return 0;
  }

  void set_status(passage_embeddings::ComputeEmbeddingsStatus status) {
    status_ = status;
  }

 private:
  passage_embeddings::ComputeEmbeddingsStatus status_ =
      passage_embeddings::ComputeEmbeddingsStatus::kSuccess;
};

class MockPageEmbeddingsService
    : public passage_embeddings::PageEmbeddingsService {
 public:
  explicit MockPageEmbeddingsService(
      page_content_annotations::PageContentExtractionService*
          page_content_extraction_service)
      : PageEmbeddingsService(page_content_extraction_service) {}
  ~MockPageEmbeddingsService() override = default;

  MOCK_METHOD(std::vector<passage_embeddings::PassageEmbedding>,
              GetEmbeddings,
              (content::WebContents * web_contents),
              (const override));

  MOCK_METHOD(void, ProcessAllEmbeddings, (), (override));

  MOCK_METHOD(void,
              AddObserver,
              (passage_embeddings::PageEmbeddingsService::Observer * observer),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (passage_embeddings::PageEmbeddingsService::Observer * observer),
              (override));
};

class MockPageContentExtractionService
    : public page_content_annotations::PageContentExtractionService {
 public:
  MockPageContentExtractionService()
      : PageContentExtractionService(nullptr, base::FilePath()) {}
  ~MockPageContentExtractionService() override = default;

  MOCK_METHOD(
      std::optional<page_content_annotations::ExtractedPageContentResult>,
      GetExtractedPageContentAndEligibilityForPage,
      (content::Page & page),
      (override));
};

class ContextualTasksContextServiceTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    InitializeFeatureList();
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    scoped_feature_list_.Reset();
    InProcessBrowserTest::TearDown();
  }

  virtual void InitializeFeatureList() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {kContextualTasksContext,
             {{{"ContextualTasksContextOnlyUseTitles", "false"},
               {"ContextualTasksContextEmbeddingSimilarityScore", "0.8"},
               {"ContextualTasksContextMinMultiSignalScore", "0.8"}}}},
            {kContextualTasksContextLogging, {}},
        },
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    InProcessBrowserTest::SetUpOnMainThread();

    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/optimization_guide");
    ASSERT_TRUE(embedded_test_server()->Start());

    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->SetModelQualityLogsUploaderServiceForTesting(
            std::make_unique<
                optimization_guide::TestModelQualityLogsUploaderService>(
                g_browser_process->local_state()));
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* browser_context) override {
    page_content_annotations::PageContentExtractionServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            browser_context,
            base::BindRepeating([](content::BrowserContext* browser_context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  testing::NiceMock<MockPageContentExtractionService>>();
            }));
    passage_embeddings::PageEmbeddingsServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            browser_context,
            base::BindRepeating([](content::BrowserContext* browser_context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  testing::NiceMock<MockPageEmbeddingsService>>(
                  page_content_annotations::
                      PageContentExtractionServiceFactory::GetForProfile(
                          Profile::FromBrowserContext(browser_context)));
            }));
    ContextualTasksContextServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            browser_context,
            base::BindRepeating(
                [](base::TickClock* test_clock,
                   passage_embeddings::EmbedderMetadataProvider*
                       embedder_metadata_provider,
                   passage_embeddings::Embedder* embedder,
                   content::BrowserContext* context)
                    -> std::unique_ptr<KeyedService> {
                  Profile* profile = Profile::FromBrowserContext(context);
                  auto service = std::make_unique<
                      ContextualTasksContextService>(
                      profile,
                      passage_embeddings::PageEmbeddingsServiceFactory::
                          GetForProfile(profile),
                      embedder_metadata_provider, embedder,
                      OptimizationGuideKeyedServiceFactory::GetForProfile(
                          profile),
                      page_content_annotations::
                          PageContentExtractionServiceFactory::GetForProfile(
                              profile));
                  service->SetClockForTesting(test_clock);
                  return service;
                },
                &test_clock_, &embedder_metadata_provider_, &embedder_));
  }

  ContextualTasksContextService* service() {
    return ContextualTasksContextServiceFactory::GetForProfile(
        browser()->profile());
  }

  MockPageEmbeddingsService* page_embeddings_service() {
    return static_cast<MockPageEmbeddingsService*>(
        passage_embeddings::PageEmbeddingsServiceFactory::GetForProfile(
            browser()->profile()));
  }

  MockPageContentExtractionService* page_content_extraction_service() {
    return static_cast<MockPageContentExtractionService*>(
        page_content_annotations::PageContentExtractionServiceFactory::
            GetForProfile(browser()->profile()));
  }

  optimization_guide::TestModelQualityLogsUploaderService* logs_uploader() {
    return static_cast<
        optimization_guide::TestModelQualityLogsUploaderService*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile())
            ->GetModelQualityLogsUploaderService());
  }

  page_content_annotations::PageContentAnnotationsService*
  page_content_annotations_service() {
    return PageContentAnnotationsServiceFactory::GetForProfile(
        browser()->profile());
  }

  void OverrideVisibilityScoresForTesting(
      const base::flat_map<std::string, double>& visibility_scores_for_input) {
    page_content_annotator_.UseVisibilityScores(std::nullopt,
                                                visibility_scores_for_input);
    page_content_annotations_service()->OverridePageContentAnnotatorForTesting(
        &page_content_annotator_);
  }

  void NotifyEmbedderMetadata() {
    embedder_metadata_provider_.NotifyObservers();
  }

  void UpdateEmbedderStatus(
      passage_embeddings::ComputeEmbeddingsStatus status) {
    embedder_.set_status(status);
  }

  passage_embeddings::Embedding CreateFakeEmbedding(float value) {
    constexpr size_t kMockPassageWordCount = 10;
    passage_embeddings::Embedding embedding(std::vector<float>(
        passage_embeddings::kEmbeddingsModelOutputSize, value));
    embedding.Normalize();
    embedding.SetPassageWordCount(kMockPassageWordCount);
    return embedding;
  }

  void NavigateToValidURL() {
    // Navigate to a valid URL.
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::NavigateToURLBlockUntilNavigationsComplete(web_contents,
                                                        valid_url(), 1);
  }

  GURL valid_url() {
    return GURL(embedded_test_server()->GetURL(
        "a.test", "/optimization_guide/hello.html"));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::SimpleTestTickClock test_clock_;

 private:
  FakeEmbedderMetadataProvider embedder_metadata_provider_;
  FakeEmbedder embedder_;
  page_content_annotations::TestPageContentAnnotator page_content_annotator_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest, NoEmbedder) {
  base::HistogramTester histogram_tester;

  NavigateToValidURL();

  base::test::TestFuture<std::vector<content::WebContents*>> future;
  service()->GetRelevantTabsForQuery(
      /*options=*/{}, "some text", /*explicit_urls=*/{}, future.GetCallback());
  EXPECT_TRUE(future.Get().empty());

  histogram_tester.ExpectTotalCount("ContextualTasks.Context.RelevantTabsCount",
                                    0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Context.ContextCalculationLatency", 0);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.ContextDeterminationStatus",
      ContextDeterminationStatus::kEmbedderNotAvailable, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest, EmbedderFailed) {
  base::HistogramTester histogram_tester;

  NavigateToValidURL();

  NotifyEmbedderMetadata();
  UpdateEmbedderStatus(
      passage_embeddings::ComputeEmbeddingsStatus::kExecutionFailure);

  base::test::TestFuture<std::vector<content::WebContents*>> future;
  service()->GetRelevantTabsForQuery(
      /*options=*/{}, "some text", /*explicit_urls=*/{}, future.GetCallback());
  EXPECT_TRUE(future.Get().empty());

  histogram_tester.ExpectTotalCount("ContextualTasks.Context.RelevantTabsCount",
                                    0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Context.ContextCalculationLatency", 0);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.ContextDeterminationStatus",
      ContextDeterminationStatus::kQueryEmbeddingFailed, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       SuccessQueryNoPageEmbeddings) {
  base::HistogramTester histogram_tester;

  NavigateToValidURL();

  NotifyEmbedderMetadata();

  base::test::TestFuture<std::vector<content::WebContents*>> future;
  service()->GetRelevantTabsForQuery(
      /*options=*/{}, "some text", /*explicit_urls=*/{}, future.GetCallback());
  EXPECT_TRUE(future.Get().empty());

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.RelevantTabsCount", 0, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Context.ContextCalculationLatency", 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.ContextDeterminationStatus",
      ContextDeterminationStatus::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest, Success) {
  base::HistogramTester histogram_tester;

  NavigateToValidURL();

  NotifyEmbedderMetadata();

  std::vector<passage_embeddings::PassageEmbedding> fake_page_embeddings = {
      // Not match.
      {std::make_pair("passage 1",
                      passage_embeddings::PassageType::kPageContent),
       CreateFakeEmbedding(0.1f)},
      // Match - active tab is added.
      {std::make_pair("passage 2",
                      passage_embeddings::PassageType::kPageContent),
       CreateFakeEmbedding(1.0f)},
      // Match - should be skipped.
      {std::make_pair("passage 3",
                      passage_embeddings::PassageType::kPageContent),
       CreateFakeEmbedding(1.0f)}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillOnce(Return(fake_page_embeddings));

  base::test::TestFuture<std::vector<content::WebContents*>> future;
  service()->GetRelevantTabsForQuery(
      {.tab_selection_mode = mojom::TabSelectionMode::kEmbeddingsMatch},
      "some text",
      /*explicit_urls=*/{valid_url()}, future.GetCallback());
  EXPECT_EQ(1u, future.Get().size());

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.RelevantTabsCount", 1, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Context.ContextCalculationLatency", 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.ContextDeterminationStatus",
      ContextDeterminationStatus::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.ExplicitTabsCount", 1, 1);
  histogram_tester.ExpectUniqueSample("ContextualTasks.Context.TabOverlapCount",
                                      1, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.TabOverlapPercentage", 100, 1);
  histogram_tester.ExpectUniqueSample("ContextualTasks.Context.TabExcessCount",
                                      0, 1);

  EXPECT_TRUE(logs_uploader()->uploaded_logs().empty());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       NotValidForServerUpload) {
  base::HistogramTester histogram_tester;

  NavigateToValidURL();

  NotifyEmbedderMetadata();

  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_)).Times(0);
  page_content_annotations::ExtractedPageContentResult result;
  result.is_eligible_for_server_upload = false;
  EXPECT_CALL(*page_content_extraction_service(),
              GetExtractedPageContentAndEligibilityForPage)
      .WillOnce(Return(result));

  base::test::TestFuture<std::vector<content::WebContents*>> future;
  service()->GetRelevantTabsForQuery(
      {.tab_selection_mode = mojom::TabSelectionMode::kEmbeddingsMatch},
      "some text",
      /*explicit_urls=*/{}, future.GetCallback());
  EXPECT_TRUE(future.Get().empty());

  histogram_tester.ExpectTotalCount("ContextualTasks.Context.RelevantTabsCount",
                                    0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Context.ContextCalculationLatency", 0);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.ContextDeterminationStatus",
      ContextDeterminationStatus::kNoEligibleTabs, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       NotValidDueToContentVisibility) {
  base::HistogramTester histogram_tester;

  NavigateToValidURL();

  NotifyEmbedderMetadata();

  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_)).Times(0);
  // Test Page is the title of valid_url().
  OverrideVisibilityScoresForTesting({{"Test Page", 0.2f}});

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", 1);

  base::test::TestFuture<std::vector<content::WebContents*>> future;
  service()->GetRelevantTabsForQuery(
      {.tab_selection_mode = mojom::TabSelectionMode::kEmbeddingsMatch},
      "some text",
      /*explicit_urls=*/{}, future.GetCallback());
  EXPECT_TRUE(future.Get().empty());

  histogram_tester.ExpectTotalCount("ContextualTasks.Context.RelevantTabsCount",
                                    0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Context.ContextCalculationLatency", 0);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.ContextDeterminationStatus",
      ContextDeterminationStatus::kNoEligibleTabs, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       VisiblePageHasNoEffect) {
  base::HistogramTester histogram_tester;

  NavigateToValidURL();

  NotifyEmbedderMetadata();

  std::vector<passage_embeddings::PassageEmbedding> fake_page_embeddings = {
      // Not match.
      {std::make_pair("passage 1",
                      passage_embeddings::PassageType::kPageContent),
       CreateFakeEmbedding(0.1f)},
      // Match - active tab is added.
      {std::make_pair("passage 2",
                      passage_embeddings::PassageType::kPageContent),
       CreateFakeEmbedding(1.0f)},
      // Match - should be skipped.
      {std::make_pair("passage 3",
                      passage_embeddings::PassageType::kPageContent),
       CreateFakeEmbedding(1.0f)}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillOnce(Return(fake_page_embeddings));
  // Test Page is the title of valid_url().
  OverrideVisibilityScoresForTesting({{"Test Page", 0.9f}});

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", 1);

  base::test::TestFuture<std::vector<content::WebContents*>> future;
  service()->GetRelevantTabsForQuery(
      {.tab_selection_mode = mojom::TabSelectionMode::kEmbeddingsMatch},
      "some text",
      /*explicit_urls=*/{valid_url()}, future.GetCallback());
  EXPECT_EQ(1u, future.Get().size());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       MultiSignalScoringMetrics) {
  base::HistogramTester histogram_tester;

  test_clock_.SetNowTicks(base::TimeTicks::Now());
  NavigateToValidURL();

  NotifyEmbedderMetadata();

  std::vector<passage_embeddings::PassageEmbedding> fake_page_embeddings = {
      {std::make_pair("passage 1",
                      passage_embeddings::PassageType::kPageContent),
       CreateFakeEmbedding(0.1f)},
      {std::make_pair("passage 2",
                      passage_embeddings::PassageType::kPageContent),
       CreateFakeEmbedding(1.0f)}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillOnce(Return(fake_page_embeddings));

  test_clock_.Advance(base::Seconds(10));

  base::test::TestFuture<void> logging_future;
  logs_uploader()->WaitForLogUpload(logging_future.GetCallback());

  base::test::TestFuture<std::vector<content::WebContents*>> future;
  service()->GetRelevantTabsForQuery(
      {.tab_selection_mode = mojom::TabSelectionMode::kMultiSignalScoring},
      "some text",
      /*explicit_urls=*/{GURL("https://notinrelevantset.com")},
      future.GetCallback());
  EXPECT_EQ(1u, future.Get().size());

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.RelevantTabsCount", 1, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Context.ContextCalculationLatency", 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.ContextDeterminationStatus",
      ContextDeterminationStatus::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.EmbeddingSimilarityScore", 99, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Context.DurationSinceLastActive", 1);
  histogram_tester.ExpectUniqueSample("ContextualTasks.Context.TabScore", 100,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.ExplicitTabsCount", 1, 1);
  histogram_tester.ExpectUniqueSample("ContextualTasks.Context.TabOverlapCount",
                                      0, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.TabOverlapPercentage", 0, 1);
  histogram_tester.ExpectUniqueSample("ContextualTasks.Context.TabExcessCount",
                                      1, 1);

  ASSERT_TRUE(logging_future.Wait());
  EXPECT_EQ(logs_uploader()->uploaded_logs().size(), 1u);
  optimization_guide::proto::ContextualTasksContextQuality
      uploaded_quality_log = logs_uploader()
                                 ->uploaded_logs()[0]
                                 ->contextual_tasks_context()
                                 .quality();
  EXPECT_EQ(uploaded_quality_log.eligible_tabs().size(), 1);
  EXPECT_GT(uploaded_quality_log.eligible_tabs()[0].best_embedding_score(),
            0.99f);
  EXPECT_GE(uploaded_quality_log.eligible_tabs()[0].seconds_since_last_active(),
            10);
  EXPECT_EQ(uploaded_quality_log.eligible_tabs()[0].number_of_common_words(),
            0);
  EXPECT_FLOAT_EQ(uploaded_quality_log.eligible_tabs()[0].aggregate_tab_score(),
                  1.0f);
  EXPECT_EQ(uploaded_quality_log.eligible_tabs()[0].was_explicitly_chosen(),
            false);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       HighEmbeddingsScoreQualifiesTab) {
  base::HistogramTester histogram_tester;

  test_clock_.SetNowTicks(base::TimeTicks::Now());
  NavigateToValidURL();

  NotifyEmbedderMetadata();

  // Passage 2 has high embeddings score.
  std::vector<passage_embeddings::PassageEmbedding> fake_page_embeddings = {
      {std::make_pair("passage 1",
                      passage_embeddings::PassageType::kPageContent),
       CreateFakeEmbedding(0.1f)},
      {std::make_pair("passage 2",
                      passage_embeddings::PassageType::kPageContent),
       CreateFakeEmbedding(1.0f)}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillOnce(Return(fake_page_embeddings));

  // Tab got old.
  test_clock_.Advance(base::Seconds(1800));

  base::test::TestFuture<std::vector<content::WebContents*>> future;
  service()->GetRelevantTabsForQuery(
      {.tab_selection_mode = mojom::TabSelectionMode::kMultiSignalScoring},
      "some text", /*explicit_urls=*/{}, future.GetCallback());
  EXPECT_EQ(1u, future.Get().size());

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.RelevantTabsCount", 1, 1);

  // Metrics comparing with explicit tabs not recorded when there are no tabs
  // chosen by the user.
  histogram_tester.ExpectTotalCount("ContextualTasks.Context.ExplicitTabsCount",
                                    0);
  histogram_tester.ExpectTotalCount("ContextualTasks.Context.TabOverlapCount",
                                    0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Context.TabOverlapPercentage", 0);
  histogram_tester.ExpectTotalCount("ContextualTasks.Context.TabExcessCount",
                                    0);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       HighRecencyScoreQualifiesTab) {
  base::HistogramTester histogram_tester;

  test_clock_.SetNowTicks(base::TimeTicks::Now());
  NavigateToValidURL();

  NotifyEmbedderMetadata();

  // None of the passages have high embeddings score.
  std::vector<passage_embeddings::PassageEmbedding> fake_page_embeddings = {
      {std::make_pair("passage 1",
                      passage_embeddings::PassageType::kPageContent),
       CreateFakeEmbedding(-1.0f)},
      {std::make_pair("passage 2",
                      passage_embeddings::PassageType::kPageContent),
       CreateFakeEmbedding(-1.0f)}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillOnce(Return(fake_page_embeddings));

  // Recently active tab.
  test_clock_.Advance(base::Seconds(5));

  base::test::TestFuture<std::vector<content::WebContents*>> future;
  service()->GetRelevantTabsForQuery(
      {.tab_selection_mode = mojom::TabSelectionMode::kMultiSignalScoring},
      "some text", /*explicit_urls=*/{}, future.GetCallback());
  EXPECT_EQ(1u, future.Get().size());

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.RelevantTabsCount", 1, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       HighLexicalMatchScoreQualifiesTab) {
  base::HistogramTester histogram_tester;

  // Navigates to a page with title "Test Page"
  NavigateToValidURL();

  NotifyEmbedderMetadata();

  base::test::TestFuture<std::vector<content::WebContents*>> future;
  service()->GetRelevantTabsForQuery(
      {.tab_selection_mode = mojom::TabSelectionMode::kMultiSignalScoring},
      "summarize the test page",
      /*explicit_urls=*/{}, future.GetCallback());

  EXPECT_EQ(1u, future.Get().size());

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.MatchingWordsCount", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.RelevantTabsCount", 1, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest, NotRelevantTab) {
  base::HistogramTester histogram_tester;

  test_clock_.SetNowTicks(base::TimeTicks::Now());
  NavigateToValidURL();

  NotifyEmbedderMetadata();

  // None of the passages have high embeddings score.
  std::vector<passage_embeddings::PassageEmbedding> fake_page_embeddings = {
      {std::make_pair("passage 1",
                      passage_embeddings::PassageType::kPageContent),
       CreateFakeEmbedding(-1.0f)},
      {std::make_pair("passage 2",
                      passage_embeddings::PassageType::kPageContent),
       CreateFakeEmbedding(-1.0f)}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillOnce(Return(fake_page_embeddings));

  // Recently active tab.
  test_clock_.Advance(base::Seconds(1800));

  base::test::TestFuture<std::vector<content::WebContents*>> future;
  service()->GetRelevantTabsForQuery(
      {.tab_selection_mode = mojom::TabSelectionMode::kMultiSignalScoring},
      "some text", /*explicit_urls=*/{}, future.GetCallback());
  EXPECT_EQ(0u, future.Get().size());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest, SkipsNonHttp) {
  base::HistogramTester histogram_tester;

  NotifyEmbedderMetadata();

  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_)).Times(0);

  base::test::TestFuture<std::vector<content::WebContents*>> future;
  service()->GetRelevantTabsForQuery(
      /*options=*/{}, "some text", /*explicit_urls=*/{}, future.GetCallback());
  EXPECT_TRUE(future.Get().empty());

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.ContextDeterminationStatus",
      ContextDeterminationStatus::kNoEligibleTabs, 1);
  histogram_tester.ExpectTotalCount("ContextualTasks.Context.RelevantTabsCount",
                                    0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Context.ContextCalculationLatency", 0);
}

class ContextualTasksContextServiceTitlesOnlyTest
    : public ContextualTasksContextServiceTest {
 public:
  // ContextualTasksContextServiceTest:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kContextualTasksContext,
          {{{"ContextualTasksContextOnlyUseTitles", "true"}}}}},
        {kContextualTasksContextLogging});
  }
};

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTitlesOnlyTest, Success) {
  NotifyEmbedderMetadata();

  NavigateToValidURL();

  std::vector<passage_embeddings::PassageEmbedding> fake_page_embeddings = {
      // Not match.
      {std::make_pair("passage 1",
                      passage_embeddings::PassageType::kPageContent),
       CreateFakeEmbedding(0.1f)},
      // Not added - page content skipped.
      {std::make_pair("passage 2",
                      passage_embeddings::PassageType::kPageContent),
       CreateFakeEmbedding(1.0f)},
      // Added - title passage matches.
      {std::make_pair("passage 3", passage_embeddings::PassageType::kTitle),
       CreateFakeEmbedding(1.0f)}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillOnce(Return(fake_page_embeddings));

  base::test::TestFuture<std::vector<content::WebContents*>> future;
  service()->GetRelevantTabsForQuery(
      /*options=*/{}, "some text", /*explicit_urls=*/{}, future.GetCallback());
  EXPECT_EQ(1u, future.Get().size());
  EXPECT_TRUE(logs_uploader()->uploaded_logs().empty());
}

}  // namespace contextual_tasks

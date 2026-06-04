// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_service.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_model_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_tab_visit_tracker.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/page_content_annotations/page_embeddings_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/prefs.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/optimization_guide/proto/tab_relevance_model_metadata.pb.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/content/page_embeddings_service.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "components/page_content_annotations/core/test_page_content_annotator.h"
#include "components/passage_embeddings/core/passage_embeddings_features.h"
#include "components/passage_embeddings/core/passage_embeddings_test_util.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/window_open_disposition.h"

namespace contextual_tasks {

using ::testing::_;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

constexpr char kValidUrlDomain[] = "a.test";

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
        passage_embeddings::EmbedderMetadata(1, 3));
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
  passage_embeddings::Embedder::Job ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority priority,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) override {
    last_passages_ = passages;
    if (status_ == passage_embeddings::ComputeEmbeddingsStatus::kSuccess) {
      if (timeout_) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(
                [](ComputePassagesEmbeddingsCallback callback,
                   std::vector<std::string> passages,
                   passage_embeddings::ComputeEmbeddingsStatus status) {
                  std::vector<passage_embeddings::Embedding> embeddings(
                      passages.size(),
                      passage_embeddings::Embedding({1.0f, 0.0f, 0.0f}));
                  std::move(callback).Run(std::move(passages),
                                          std::move(embeddings), 1, status);
                },
                std::move(callback), std::move(passages), status_),
            *timeout_);
        return passage_embeddings::Embedder::Job(GetWeakPtr(), 1);
      }

      return passage_embeddings::TestEmbedder::ComputePassagesEmbeddings(
          priority, std::move(passages), std::move(callback));
    }

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(passages),
                                  std::vector<passage_embeddings::Embedding>(),
                                  1, status_));
    return passage_embeddings::Embedder::Job(GetWeakPtr(), 1);
  }

  void set_status(passage_embeddings::ComputeEmbeddingsStatus status) {
    status_ = status;
  }

  void set_timeout(base::TimeDelta timeout) { timeout_ = timeout; }

  const std::vector<std::string>& last_passages() const {
    return last_passages_;
  }

 private:
  passage_embeddings::ComputeEmbeddingsStatus status_ =
      passage_embeddings::ComputeEmbeddingsStatus::kSuccess;
  std::optional<base::TimeDelta> timeout_;
  std::vector<std::string> last_passages_;
};

class MockPageEmbeddingsService
    : public page_content_annotations::PageEmbeddingsService {
 public:
  explicit MockPageEmbeddingsService(
      page_content_annotations::PageContentExtractionService*
          page_content_extraction_service)
      : PageEmbeddingsService(page_content_extraction_service) {}
  ~MockPageEmbeddingsService() override = default;

  MOCK_METHOD(std::vector<page_content_annotations::PassageEmbedding>,
              GetEmbeddings,
              (content::Page & page),
              (const override));

  MOCK_METHOD(void, ProcessEmbeddingsOnDemand, (), (override));

  MOCK_METHOD(void,
              AddObserver,
              (page_content_annotations::PageEmbeddingsService::Observer *
               observer),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (page_content_annotations::PageEmbeddingsService::Observer *
               observer),
              (override));
};

class MockPageContentExtractionService
    : public page_content_annotations::PageContentExtractionService {
 public:
  MockPageContentExtractionService()
      : PageContentExtractionService(nullptr, base::FilePath(), nullptr) {}
  ~MockPageContentExtractionService() override = default;

  MOCK_METHOD(std::optional<bool>,
              GetServerUploadEligibilityForPage,
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
    InProcessBrowserTest::TearDown();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("ignore-google-port-numbers");
  }

  virtual void InitializeFeatureList() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {kContextualTasksContext,
             {{{"ContextualTasksContextOnlyUseTitles", "false"},
               {"ContextualTasksContextDeduplicateByUrl", "false"},
               {"ContextualTasksContextTabSelectionScoreThreshold", "0.8"},
               {"ContextualTasksContextContentVisibilityThreshold", "0.8"}}}},
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
    page_content_annotations::PageEmbeddingsServiceFactory::GetInstance()
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
                      page_content_annotations::PageEmbeddingsServiceFactory::
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

  FakeEmbedder& embedder() { return embedder_; }

  ContextualTasksContextModelHandler* GetModelHandler() {
    return service()->model_handler_.get();
  }

  void UpdateModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const optimization_guide::ModelInfo& model_info) {
    service()->model_handler_ =
        std::make_unique<ContextualTasksContextModelHandler>(
            OptimizationGuideKeyedServiceFactory::GetForProfile(
                browser()->profile()),
            base::SequencedTaskRunner::GetCurrentDefault());
    service()->model_handler_->OnModelUpdated(optimization_target, model_info);
  }

  MockPageEmbeddingsService* page_embeddings_service() {
    return static_cast<MockPageEmbeddingsService*>(
        page_content_annotations::PageEmbeddingsServiceFactory::GetForProfile(
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

  void UpdateEmbedderTimeout(base::TimeDelta timeout) {
    embedder_.set_timeout(timeout);
  }

  void NavigateToValidURL() {
    // Navigate to a valid URL.
    tabs::TabInterface* tab = TabListInterface::From(browser())->GetActiveTab();
    if (!tab) {
      return;
    }
    if (auto* tracker = ContextualTasksTabVisitTracker::From(tab)) {
      tracker->SetClockForTesting(&test_clock_);
    }
    content::NavigateToURLBlockUntilNavigationsComplete(tab->GetContents(),
                                                        valid_url(), 1);
  }

  GURL valid_url() {
    return GURL(embedded_test_server()->GetURL(
        kValidUrlDomain, "/optimization_guide/hello.html"));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::SimpleTestTickClock test_clock_;

 private:
  FakeEmbedderMetadataProvider embedder_metadata_provider_;
  FakeEmbedder embedder_;
  page_content_annotations::TestPageContentAnnotator page_content_annotator_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest, EmptyQuery) {
  base::HistogramTester histogram_tester;

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  service()->GetRelevantTabsForQuery(
      /*options=*/{}, "", /*explicit_urls=*/{}, future.GetCallback());
  EXPECT_TRUE(future.Get().empty());

  histogram_tester.ExpectTotalCount("ContextualTasks.Context.RelevantTabsCount",
                                    0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Context.ContextCalculationLatency", 0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Context.ContextDeterminationStatus", 0);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest, NoEmbedder) {
  base::HistogramTester histogram_tester;

  NavigateToValidURL();

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
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

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
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

class ContextualTasksContextServicePreviousTabSignalTest
    : public ContextualTasksContextServiceTest {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {kContextualTasksContext,
             {{{"ContextualTasksContextOnlyUseTitles", "false"},
               {"ContextualTasksContextTabSelectionScoreThreshold", "0.8"},
               {"ContextualTasksContextContentVisibilityThreshold", "0.8"},
               {"ContextualTasksEnablePreviousTabFallback", "true"}}}},
            {kContextualTasksContextLogging, {}},
        },
        /*disabled_features=*/{});
  }
};

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServicePreviousTabSignalTest,
                       FallbackToPreviousTab) {
  base::HistogramTester histogram_tester;

  NavigateToValidURL();
  test_clock_.Advance(base::Seconds(60));

  // Open invalid tab.
  GURL ntp_url("chrome://new-tab-page/");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), ntp_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  tabs::TabInterface* tab_b = TabListInterface::From(browser())->GetActiveTab();
  if (auto* tracker = ContextualTasksTabVisitTracker::From(tab_b)) {
    tracker->SetClockForTesting(&test_clock_);
  }

  tabs::TabInterface* tab_a = TabListInterface::From(browser())->GetTab(0);
  ASSERT_NE(tab_a, nullptr);
  if (auto* tracker = ContextualTasksTabVisitTracker::From(tab_a)) {
    tracker->SetClockForTesting(&test_clock_);
  }
  tab_a->GetContents()->WasShown();
  tab_a->GetContents()->WasHidden();

  test_clock_.Advance(base::Seconds(5));

  std::vector<page_content_annotations::PassageEmbedding> fake_page_embeddings =
      {{std::make_pair("page title",
                       page_content_annotations::EmbeddingPassageType::kTitle),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})}};

  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillRepeatedly(Return(fake_page_embeddings));

  NotifyEmbedderMetadata();

  base::test::TestFuture<void> logging_future;
  logs_uploader()->WaitForLogUpload(logging_future.GetCallback());

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kEmbeddingsMatch;

  service()->GetRelevantTabsForQuery(options, "some text",
                                     /*explicit_urls=*/{},
                                     future.GetCallback());

  // Wait for result.
  ASSERT_TRUE(future.Wait());

  ASSERT_TRUE(logging_future.Wait());
  EXPECT_EQ(logs_uploader()->uploaded_logs().size(), 1u);
  optimization_guide::proto::ContextualTasksContextQuality
      uploaded_quality_log = logs_uploader()
                                 ->uploaded_logs()[0]
                                 ->contextual_tasks_context()
                                 .quality();

  // Tab A was used for obtaining active_tab features i.e. to contextualize the
  // query. Its title embedding ({1,0,0}) matches query embedding ({1,0,0}).
  // So similarity should be 1.0.
  EXPECT_EQ(uploaded_quality_log.query_active_tab_title_similarity(), 1.0f);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       SuccessQueryNoPageEmbeddings) {
  base::HistogramTester histogram_tester;

  NavigateToValidURL();

  NotifyEmbedderMetadata();

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
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

  std::vector<page_content_annotations::PassageEmbedding> fake_page_embeddings =
      {// Not match.
       {std::make_pair(
            "passage 1",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})},
       // Match - active tab is added.
       {std::make_pair(
            "passage 2",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})},
       // Match - should be skipped.
       {std::make_pair(
            "passage 3",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillRepeatedly(Return(fake_page_embeddings));

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kEmbeddingsMatch;
  service()->GetRelevantTabsForQuery(options, "some text",
                                     /*explicit_urls=*/{valid_url()},
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
      "ContextualTasks.Context.ExplicitTabsCount", 1, 1);
  histogram_tester.ExpectUniqueSample("ContextualTasks.Context.TabOverlapCount",
                                      1, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.TabOverlapPercentage", 100, 1);
  histogram_tester.ExpectUniqueSample("ContextualTasks.Context.TabExcessCount",
                                      0, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       FiltersGoogleSearchAndHome) {
  // 1. Navigate to a valid URL (a.test).
  NavigateToValidURL();

  // 2. Open a new tab and navigate to Google Search.
  GURL search_url =
      embedded_test_server()->GetURL("www.google.com", "/search?q=test");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), search_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // 3. Open another new tab and navigate to Google Home Page.
  GURL home_url = embedded_test_server()->GetURL("www.google.com", "/");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), home_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  NotifyEmbedderMetadata();

  // Set up embeddings for the valid tab.
  std::vector<page_content_annotations::PassageEmbedding> fake_page_embeddings =
      {{std::make_pair(
            "passage 1",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillRepeatedly(Return(fake_page_embeddings));

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kEmbeddingsMatch;
  service()->GetRelevantTabsForQuery(options, "some text",
                                     /*explicit_urls=*/{},
                                     future.GetCallback());

  std::vector<base::WeakPtr<content::WebContents>> result = future.Get();
  EXPECT_EQ(1u, result.size());
  if (!result.empty()) {
    EXPECT_EQ(result[0]->GetLastCommittedURL(), valid_url());
  }
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       ActiveTabAndQueryRaceCondition) {
  NavigateToValidURL();

  GURL url_b = embedded_test_server()->GetURL(
      kValidUrlDomain, "/optimization_guide/hello.html?b");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url_b, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  browser()->tab_strip_model()->ActivateTabAt(0);

  NotifyEmbedderMetadata();

  std::vector<page_content_annotations::PassageEmbedding>
      fake_page_embeddings_a = {
          {std::make_pair(
               "page title a",
               page_content_annotations::EmbeddingPassageType::kTitle),
           passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})}};
  std::vector<page_content_annotations::PassageEmbedding>
      fake_page_embeddings_b = {
          {std::make_pair(
               "page title b",
               page_content_annotations::EmbeddingPassageType::kTitle),
           passage_embeddings::Embedding({0.0f, 1.0f, 0.0f})}};

  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillRepeatedly([&](const content::Page& page) {
        if (const_cast<content::Page&>(page)
                .GetMainDocument()
                .GetLastCommittedURL() == valid_url()) {
          return fake_page_embeddings_a;
        }
        return fake_page_embeddings_b;
      });

  UpdateEmbedderTimeout(base::Milliseconds(500));

  base::test::TestFuture<void> logging_future;
  logs_uploader()->WaitForLogUpload(logging_future.GetCallback());

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kEmbeddingsMatch;

  service()->GetRelevantTabsForQuery(options, "some text",
                                     /*explicit_urls=*/{},
                                     future.GetCallback());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](Browser* browser) {
            browser->tab_strip_model()->ActivateTabAt(1);
          },
          browser()),
      base::Milliseconds(100));

  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(logging_future.Wait());

  EXPECT_EQ(logs_uploader()->uploaded_logs().size(), 1u);
  optimization_guide::proto::ContextualTasksContextQuality
      uploaded_quality_log = logs_uploader()
                                 ->uploaded_logs()[0]
                                 ->contextual_tasks_context()
                                 .quality();

  EXPECT_EQ(uploaded_quality_log.query_active_tab_title_similarity(), 1.0f);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       SuccessForConversationWithSingleTurnFlow) {
  base::HistogramTester histogram_tester;

  NavigateToValidURL();

  NotifyEmbedderMetadata();

  std::vector<page_content_annotations::PassageEmbedding> fake_page_embeddings =
      {// Not match.
       {std::make_pair(
            "passage 1",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})},
       // Match - active tab is added.
       {std::make_pair(
            "passage 2",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})},
       // Match - should be skipped.
       {std::make_pair(
            "passage 3",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillRepeatedly(Return(fake_page_embeddings));

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kEmbeddingsMatch;

  ConversationThread conversation_thread;
  ThreadTurn turn1;
  turn1.query = "history query";
  conversation_thread.previous_turns.push_back(turn1);

  conversation_thread.query = "some text";
  conversation_thread.shared_tab_titles.push_back("shared tab 1");
  conversation_thread.shared_tab_titles.push_back("shared tab 2");

  service()->GetRelevantTabsForConversationThread(options, conversation_thread,
                                            /*explicit_urls=*/{valid_url()},
                                            future.GetCallback());
  EXPECT_EQ(1u, future.Get().size());

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.RelevantTabsCount", 1, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Context.ContextCalculationLatency", 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.ContextDeterminationStatus",
      ContextDeterminationStatus::kSuccess, 1);
}

class ContextualTasksContextServiceTaskFormattingTest
    : public ContextualTasksContextServiceTest {
 protected:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {kContextualTasksContext,
             {{"ContextualTasksContextQueryEmbeddingTask", "search result"},
              {"ContextualTasksContextOnlyUseTitles", "false"},
              {"ContextualTasksContextTabSelectionScoreThreshold", "0.8"},
              {"ContextualTasksContextContentVisibilityThreshold", "0.8"}}},
            {kContextualTasksContextLogging, {}},
        },
        /*disabled_features=*/{});
  }
};

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTaskFormattingTest,
                       SuccessWithTaskFormatting) {
  NavigateToValidURL();
  NotifyEmbedderMetadata();

  std::vector<page_content_annotations::PassageEmbedding> fake_page_embeddings =
      {{std::make_pair(
            "passage 1",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})},
       {std::make_pair(
            "passage 2",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillRepeatedly(Return(fake_page_embeddings));

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kEmbeddingsMatch;
  service()->GetRelevantTabsForQuery(options, "some text",
                                     /*explicit_urls=*/{valid_url()},
                                     future.GetCallback());
  EXPECT_EQ(1u, future.Get().size());

  // Verify the formatted query was sent to the embedder.
  const auto& last_passages = embedder().last_passages();
  ASSERT_EQ(1u, last_passages.size());
  EXPECT_EQ("task: search result | query: some text", last_passages[0]);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest, FiltersForWindow) {
  BrowserWindowInterface* first_window = browser();
  // Navigate to a valid URL in the current window.
  NavigateToValidURL();

  NotifyEmbedderMetadata();

  // Create a new browser window.
  BrowserWindowInterface* new_browser =
      CreateBrowserWindow(BrowserWindowCreateParams(
          *browser()->profile(), /*from_user_gesture=*/false));
  ASSERT_TRUE(new_browser);

  {
    base::HistogramTester histogram_tester;

    // No tabs in the new window, so no embeddings should be requested.
    EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_)).Times(0);

    base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
        future;
    TabSelectionOptions options;
    options.tab_selection_mode = mojom::TabSelectionMode::kEmbeddingsMatch;
    options.browser_window_interface = new_browser->GetWeakPtr();
    service()->GetRelevantTabsForQuery(options, "some text",
                                       /*explicit_urls=*/{valid_url()},
                                       future.GetCallback());
    EXPECT_TRUE(future.Get().empty());

    histogram_tester.ExpectUniqueSample(
        "ContextualTasks.Context.ContextDeterminationStatus",
        ContextDeterminationStatus::kNoEligibleTabs, 1);
  }

  {
    base::HistogramTester histogram_tester;

    std::vector<page_content_annotations::PassageEmbedding>
        fake_page_embeddings = {
            // Not match.
            {std::make_pair(
                 "passage 1",
                 page_content_annotations::EmbeddingPassageType::kPageContent),
             passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})},
            // Match - active tab is added.
            {std::make_pair(
                 "passage 2",
                 page_content_annotations::EmbeddingPassageType::kPageContent),
             passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})},
            // Match - should be skipped.
            {std::make_pair(
                 "passage 3",
                 page_content_annotations::EmbeddingPassageType::kPageContent),
             passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})}};
    EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
        .WillRepeatedly(Return(fake_page_embeddings));

    base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
        future;
    TabSelectionOptions options;
    options.tab_selection_mode = mojom::TabSelectionMode::kEmbeddingsMatch;
    options.browser_window_interface = first_window->GetWeakPtr();
    service()->GetRelevantTabsForQuery(options, "some text",
                                       /*explicit_urls=*/{valid_url()},
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
        "ContextualTasks.Context.ExplicitTabsCount", 1, 1);
    histogram_tester.ExpectUniqueSample(
        "ContextualTasks.Context.TabOverlapCount", 1, 1);
    histogram_tester.ExpectUniqueSample(
        "ContextualTasks.Context.TabOverlapPercentage", 100, 1);
    histogram_tester.ExpectUniqueSample(
        "ContextualTasks.Context.TabExcessCount", 0, 1);
  }
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       SuccessWithTimeoutSpecified) {
  base::HistogramTester histogram_tester;

  NavigateToValidURL();

  NotifyEmbedderMetadata();
  // Set timeout shorter than the request timeout.
  UpdateEmbedderTimeout(base::Milliseconds(100));

  std::vector<page_content_annotations::PassageEmbedding> fake_page_embeddings =
      {// Not match.
       {std::make_pair(
            "passage 1",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})},
       // Match - active tab is added.
       {std::make_pair(
            "passage 2",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})},
       // Match - should be skipped.
       {std::make_pair(
            "passage 3",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillOnce(Return(fake_page_embeddings));

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kEmbeddingsMatch;
  options.tab_selection_timeout = base::Seconds(1);
  service()->GetRelevantTabsForQuery(options, "some text",
                                     /*explicit_urls=*/{valid_url()},
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
      "ContextualTasks.Context.ExplicitTabsCount", 1, 1);
  histogram_tester.ExpectUniqueSample("ContextualTasks.Context.TabOverlapCount",
                                      1, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.TabOverlapPercentage", 100, 1);
  histogram_tester.ExpectUniqueSample("ContextualTasks.Context.TabExcessCount",
                                      0, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest, TimedOut) {
  base::HistogramTester histogram_tester;

  NavigateToValidURL();

  NotifyEmbedderMetadata();
  // Set timeout longer than the request timeout.
  UpdateEmbedderTimeout(base::Milliseconds(200));

  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_)).Times(0);

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kEmbeddingsMatch;
  options.tab_selection_timeout = base::Milliseconds(100);
  service()->GetRelevantTabsForQuery(options, "some text",
                                     /*explicit_urls=*/{valid_url()},
                                     future.GetCallback());
  EXPECT_TRUE(future.Get().empty());

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.ContextDeterminationStatus",
      ContextDeterminationStatus::kTimedOut, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       TimedOutDuringTabScoring) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::HistogramTester histogram_tester;

  // unit_test_tab_relevance.tflite expects 25 float inputs.
  optimization_guide::proto::TabRelevanceModelMetadata metadata;
  metadata.set_num_features(25);
  metadata.set_num_passages_per_tab(10);
  metadata.add_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_LENGTH);
  metadata.add_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_TITLE_LEXICAL_SIMILARITY);
  metadata.add_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_ACTIVE_TAB_SIMILARITY);
  metadata.add_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_CANDIDATE_TAB_SIMILARITY);
  metadata.add_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_ACTIVE_CANDIDATE_TAB_SIMILARITY);

  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/"
      "optimization_guide.proto.TabRelevanceModelMetadata");
  metadata.SerializeToString(any_metadata.mutable_value());

  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
  base::FilePath model_file_path =
      test_data_dir.AppendASCII("components")
          .AppendASCII("test")
          .AppendASCII("data")
          .AppendASCII("contextual_tasks")
          .AppendASCII("unit_test_tab_relevance.tflite");
  {
    ASSERT_TRUE(base::PathExists(model_file_path));
  }

  auto model_info = optimization_guide::TestModelInfoBuilder()
                        .SetModelFilePath(model_file_path)
                        .SetModelMetadata(any_metadata)
                        .Build();
  UpdateModel(optimization_guide::proto::
                  OPTIMIZATION_TARGET_CONTEXTUAL_TASKS_TAB_RELEVANCE,
              *model_info);

  NavigateToValidURL();

  NotifyEmbedderMetadata();
  // Embedder solves instantly.
  UpdateEmbedderTimeout(base::Milliseconds(0));

  std::vector<page_content_annotations::PassageEmbedding> fake_page_embeddings =
      {{std::make_pair("page title",
                       page_content_annotations::EmbeddingPassageType::kTitle),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillRepeatedly(Return(fake_page_embeddings));

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kStaticSignalsMlModel;
  options.min_model_score = 0.5f;
  // Set request timeout to 1ms so that during asynchronous model execution,
  // the timeout task is guaranteed to run first.
  options.tab_selection_timeout = base::Milliseconds(1);

  service()->GetRelevantTabsForQuery(options, "summarize the test page now",
                                     /*explicit_urls=*/{},
                                     future.GetCallback());

  // The request should time out and return no relevant tabs.
  EXPECT_TRUE(future.Get().empty());

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.ContextDeterminationStatus",
      ContextDeterminationStatus::kTimedOut, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       NotValidForServerUpload) {
  base::HistogramTester histogram_tester;

  NavigateToValidURL();

  NotifyEmbedderMetadata();

  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_)).Times(0);
  EXPECT_CALL(*page_content_extraction_service(),
              GetServerUploadEligibilityForPage)
      .WillOnce(Return(false));

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kEmbeddingsMatch;
  service()->GetRelevantTabsForQuery(options, "some text",
                                     /*explicit_urls=*/{},
                                     future.GetCallback());
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

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kEmbeddingsMatch;
  service()->GetRelevantTabsForQuery(options, "some text",
                                     /*explicit_urls=*/{},
                                     future.GetCallback());
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

  std::vector<page_content_annotations::PassageEmbedding> fake_page_embeddings =
      {// Not match.
       {std::make_pair(
            "passage 1",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})},
       // Match - active tab is added.
       {std::make_pair(
            "passage 2",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})},
       // Match - should be skipped.
       {std::make_pair(
            "passage 3",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillRepeatedly(Return(fake_page_embeddings));
  // Test Page is the title of valid_url().
  OverrideVisibilityScoresForTesting({{"Test Page", 0.9f}});

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", 1);

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kEmbeddingsMatch;
  service()->GetRelevantTabsForQuery(options, "some text",
                                     /*explicit_urls=*/{valid_url()},
                                     future.GetCallback());
  EXPECT_EQ(1u, future.Get().size());
}

struct ContextualTasksTestParams {
  mojom::TabSelectionMode mode;
  int expected_tab_score_bucket;
};

class ContextualTasksContextServiceParameterizedTest
    : public ContextualTasksContextServiceTest,
      public testing::WithParamInterface<ContextualTasksTestParams> {};

IN_PROC_BROWSER_TEST_P(ContextualTasksContextServiceParameterizedTest,
                       LogSignalsAndMetricsInAllTabSelectionModes) {
  const ContextualTasksTestParams& params = GetParam();
  base::HistogramTester histogram_tester;

  test_clock_.SetNowTicks(base::TimeTicks::Now());
  // Navigates to a page with title "Test Page"
  NavigateToValidURL();
  // Simulate a long time spent on the tab.
  test_clock_.Advance(base::Seconds(60));
  // Simulate a short time passed since the tab has been hidden.
  TabListInterface::From(browser())->GetActiveTab()->GetContents()->WasHidden();
  test_clock_.Advance(base::Seconds(3));

  NotifyEmbedderMetadata();

  std::vector<page_content_annotations::PassageEmbedding> fake_page_embeddings =
      {{std::make_pair(
            "passage 1",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})},
       {std::make_pair(
            "passage 2",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})},
       {std::make_pair("page title",
                       page_content_annotations::EmbeddingPassageType::kTitle),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillRepeatedly(Return(fake_page_embeddings));

  base::test::TestFuture<void> logging_future;
  logs_uploader()->WaitForLogUpload(logging_future.GetCallback());

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = params.mode;
  service()->GetRelevantTabsForQuery(
      options, "summarize the test page",
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
      "ContextualTasks.Context.EmbeddingSimilarityScore", 100, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Context.DurationSinceLastActive", 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.MatchingWordsCount", 2, 1);
  histogram_tester.ExpectUniqueSample("ContextualTasks.Context.TabScore",
                                      params.expected_tab_score_bucket, 1);
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
  EXPECT_EQ(uploaded_quality_log.number_of_query_words(), 4);
  EXPECT_EQ(uploaded_quality_log.query_active_tab_title_similarity(), 1.0f);
  EXPECT_EQ(uploaded_quality_log.query_active_tab_passage_similarities().size(),
            2);
  EXPECT_GE(uploaded_quality_log.query_active_tab_passage_similarities()[0],
            1.0f);
  EXPECT_EQ(uploaded_quality_log.eligible_tabs().size(), 1);
  EXPECT_GE(uploaded_quality_log.eligible_tabs()[0].best_embedding_score(),
            1.0f);
  EXPECT_GT(uploaded_quality_log.eligible_tabs()[0].query_title_similarity(),
            0.0f);
  EXPECT_GT(uploaded_quality_log.eligible_tabs()[0]
                .query_passage_similarities()
                .size(),
            0);
  EXPECT_GE(
      uploaded_quality_log.eligible_tabs()[0].active_tab_title_similarity(),
      1.0f);
  EXPECT_EQ(uploaded_quality_log.eligible_tabs()[0].seconds_since_last_active(),
            3);
  EXPECT_GE(uploaded_quality_log.eligible_tabs()[0].seconds_of_last_visit(),
            60);
  EXPECT_EQ(uploaded_quality_log.eligible_tabs()[0].number_of_common_words(),
            2);
  EXPECT_NEAR(uploaded_quality_log.eligible_tabs()[0].aggregate_tab_score(),
              1.0f, 0.001f);
  EXPECT_EQ(uploaded_quality_log.eligible_tabs()[0].was_explicitly_chosen(),
            false);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksContextServiceParameterizedTest,
    testing::Values(
        ContextualTasksTestParams{mojom::TabSelectionMode::kEmbeddingsMatch,
                                  100},
        ContextualTasksTestParams{mojom::TabSelectionMode::kStaticSignalsOnly,
                                  100},
        ContextualTasksTestParams{mojom::TabSelectionMode::kMultiSignalScoring,
                                  100}));

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       MultiSignalScoring_HighEmbeddingsScoreQualifiesTab) {
  base::HistogramTester histogram_tester;

  test_clock_.SetNowTicks(base::TimeTicks::Now());
  NavigateToValidURL();
  // Simulate low time spent on the tab.
  test_clock_.Advance(base::Seconds(3));
  // Simulate a long time passed since the tab has been hidden.
  TabListInterface::From(browser())->GetActiveTab()->GetContents()->WasHidden();
  test_clock_.Advance(base::Seconds(1800));

  NotifyEmbedderMetadata();

  // Passage 2 has high embeddings score.
  std::vector<page_content_annotations::PassageEmbedding> fake_page_embeddings =
      {{std::make_pair(
            "passage 1",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})},
       {std::make_pair(
            "passage 2",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillRepeatedly(Return(fake_page_embeddings));

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kMultiSignalScoring;
  service()->GetRelevantTabsForQuery(options, "some text", /*explicit_urls=*/{},
                                     future.GetCallback());
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
                       MultiSignalScoring_HighRecencyLongDurationQualifiesTab) {
  base::HistogramTester histogram_tester;

  test_clock_.SetNowTicks(base::TimeTicks::Now());
  NavigateToValidURL();
  // Simulate a long time spent on the tab.
  test_clock_.Advance(base::Seconds(60));
  // Simulate a short time passed since the tab has been hidden.
  TabListInterface::From(browser())->GetActiveTab()->GetContents()->WasHidden();
  test_clock_.Advance(base::Seconds(3));

  NotifyEmbedderMetadata();

  // None of the passages have high embeddings score.
  std::vector<page_content_annotations::PassageEmbedding> fake_page_embeddings =
      {{std::make_pair(
            "passage 1",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({-1.0f, 0.0f, 0.0f})},
       {std::make_pair(
            "passage 2",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({-1.0f, 0.0f, 0.0f})}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillRepeatedly(Return(fake_page_embeddings));

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kMultiSignalScoring;
  service()->GetRelevantTabsForQuery(options, "some text", /*explicit_urls=*/{},
                                     future.GetCallback());
  EXPECT_EQ(1u, future.Get().size());

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.RelevantTabsCount", 1, 1);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksContextServiceTest,
    MultiSignalScoring_HighRecencyShortDurationDoesNotQualifyTab) {
  base::HistogramTester histogram_tester;

  test_clock_.SetNowTicks(base::TimeTicks::Now());
  NavigateToValidURL();
  // Simulate a short time spent on the tab.
  test_clock_.Advance(base::Seconds(3));
  // Simulate a short time passed since the tab has been hidden.
  TabListInterface::From(browser())->GetActiveTab()->GetContents()->WasHidden();
  test_clock_.Advance(base::Seconds(3));

  NotifyEmbedderMetadata();

  // None of the passages have high embeddings score.
  std::vector<page_content_annotations::PassageEmbedding> fake_page_embeddings =
      {{std::make_pair(
            "passage 1",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({-1.0f, 0.0f, 0.0f})},
       {std::make_pair(
            "passage 2",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({-1.0f, 0.0f, 0.0f})}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillRepeatedly(Return(fake_page_embeddings));

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kMultiSignalScoring;
  service()->GetRelevantTabsForQuery(options, "some text", /*explicit_urls=*/{},
                                     future.GetCallback());
  EXPECT_EQ(0u, future.Get().size());

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.RelevantTabsCount", 0, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       MultiSignalScoring_HighLexicalMatchScoreQualifiesTab) {
  base::HistogramTester histogram_tester;

  test_clock_.SetNowTicks(base::TimeTicks::Now());
  // Navigates to a page with title "Test Page"
  NavigateToValidURL();
  // Simulate a short time spent on the tab.
  test_clock_.Advance(base::Seconds(3));
  // Simulate a long time passed since the tab has been hidden.
  TabListInterface::From(browser())->GetActiveTab()->GetContents()->WasHidden();
  test_clock_.Advance(base::Seconds(1800));

  NotifyEmbedderMetadata();

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kMultiSignalScoring;
  service()->GetRelevantTabsForQuery(options, "summarize the test page",
                                     /*explicit_urls=*/{},
                                     future.GetCallback());

  EXPECT_EQ(1u, future.Get().size());

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.MatchingWordsCount", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.RelevantTabsCount", 1, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       MultiSignalScoring_NotRelevantTab) {
  base::HistogramTester histogram_tester;

  test_clock_.SetNowTicks(base::TimeTicks::Now());
  NavigateToValidURL();
  // Simulate a short time spent on the tab.
  test_clock_.Advance(base::Seconds(3));
  // Simulate a long time passed since the tab has been hidden.
  TabListInterface::From(browser())->GetActiveTab()->GetContents()->WasHidden();
  test_clock_.Advance(base::Seconds(1800));

  NotifyEmbedderMetadata();

  // None of the passages have high embeddings score.
  std::vector<page_content_annotations::PassageEmbedding> fake_page_embeddings =
      {{std::make_pair(
            "passage 1",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({-1.0f, 0.0f, 0.0f})},
       {std::make_pair(
            "passage 2",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({-1.0f, 0.0f, 0.0f})}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillRepeatedly(Return(fake_page_embeddings));

  // Recently active tab.
  test_clock_.Advance(base::Seconds(1800));

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kMultiSignalScoring;
  service()->GetRelevantTabsForQuery(options, "some text", /*explicit_urls=*/{},
                                     future.GetCallback());
  EXPECT_EQ(0u, future.Get().size());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest, SkipsNonHttp) {
  base::HistogramTester histogram_tester;

  NotifyEmbedderMetadata();

  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_)).Times(0);

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
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

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       SignalPopulation_ActiveTabNTP) {
  test_clock_.SetNowTicks(base::TimeTicks::Now());

  // Tab 1 (will become background candidate tab)
  NavigateToValidURL();

  // Set up an NTP as the active foreground tab.
  GURL ntp_url("chrome://newtab/");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), ntp_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  NotifyEmbedderMetadata();

  std::vector<page_content_annotations::PassageEmbedding> fake_page_embeddings =
      {{std::make_pair(
            "passage 1",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})},
       {std::make_pair("candidate title",
                       page_content_annotations::EmbeddingPassageType::kTitle),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillRepeatedly(Return(fake_page_embeddings));

  base::test::TestFuture<void> logging_future;
  logs_uploader()->WaitForLogUpload(logging_future.GetCallback());

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kMultiSignalScoring;
  service()->GetRelevantTabsForQuery(options, "summarize the test page",
                                     /*explicit_urls=*/{},
                                     future.GetCallback());

  ASSERT_TRUE(logging_future.Wait());

  optimization_guide::proto::ContextualTasksContextQuality
      uploaded_quality_log = logs_uploader()
                                 ->uploaded_logs()[0]
                                 ->contextual_tasks_context()
                                 .quality();

  ASSERT_GE(uploaded_quality_log.eligible_tabs().size(), 1);

  // The active tab is NTP, so we should not have fetched embeddings for it,
  // meaning active_tab_title_similarity should be 0 (default).
  EXPECT_EQ(
      uploaded_quality_log.eligible_tabs()[0].active_tab_title_similarity(),
      0.0f);
  EXPECT_EQ(uploaded_quality_log.eligible_tabs()[0]
                .query_passage_similarities()
                .size(),
            1);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       SignalPopulation_MissingTitleEmbeddings) {
  test_clock_.SetNowTicks(base::TimeTicks::Now());
  NavigateToValidURL();
  NotifyEmbedderMetadata();

  // Mock embeddings: Provide only page content, no kTitle passage.
  std::vector<page_content_annotations::PassageEmbedding> fake_page_embeddings =
      {{std::make_pair(
            "passage 1",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})}};

  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillRepeatedly(Return(fake_page_embeddings));

  base::test::TestFuture<void> logging_future;
  logs_uploader()->WaitForLogUpload(logging_future.GetCallback());

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kMultiSignalScoring;
  service()->GetRelevantTabsForQuery(options, "summarize the test page",
                                     /*explicit_urls=*/{},
                                     future.GetCallback());

  ASSERT_TRUE(logging_future.Wait());

  optimization_guide::proto::ContextualTasksContextQuality
      uploaded_quality_log = logs_uploader()
                                 ->uploaded_logs()[0]
                                 ->contextual_tasks_context()
                                 .quality();

  EXPECT_EQ(uploaded_quality_log.query_active_tab_title_similarity(), 0.0f);
  ASSERT_EQ(uploaded_quality_log.query_active_tab_passage_similarities().size(),
            1);

  ASSERT_EQ(uploaded_quality_log.eligible_tabs().size(), 1);
  auto& tab_context = uploaded_quality_log.eligible_tabs()[0];

  // Verify that missing title embeddings default to 0.0f without crashing.
  EXPECT_EQ(tab_context.query_title_similarity(), 0.0f);
  EXPECT_EQ(tab_context.active_tab_title_similarity(), 0.0f);

  // Verify that passage similarities are still correctly extracted and
  // populated.
  ASSERT_EQ(tab_context.query_passage_similarities().size(), 1);
  EXPECT_NEAR(tab_context.query_passage_similarities()[0], 1.0f, 0.01f);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       SignalPopulation_CandidateDifferentFromActiveTab) {
  test_clock_.SetNowTicks(base::TimeTicks::Now());

  // Tab 1 (will become background candidate tab)
  NavigateToValidURL();

  // Tab 2 (will become the active foreground tab)
  GURL url2 = embedded_test_server()->GetURL("b.test",
                                             "/optimization_guide/hello.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  NotifyEmbedderMetadata();

  // Mock different embeddings to differentiate Active Tab vs Candidate Tab.
  int call_count = 0;
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillRepeatedly([&call_count](content::Page& page) {
        std::vector<page_content_annotations::PassageEmbedding> embeddings;
        if (call_count++ == 0) {
          // First call is for the Active Tab (in CreateQueryState)
          embeddings.emplace_back(
              std::make_pair(
                  "active title",
                  page_content_annotations::EmbeddingPassageType::kTitle),
              passage_embeddings::Embedding({-1.0f, 0.0f, 0.0f}));
        } else {
          // Subsequent calls are for the candidate tabs in the all_tabs loop
          embeddings.emplace_back(
              std::make_pair(
                  "candidate title",
                  page_content_annotations::EmbeddingPassageType::kTitle),
              passage_embeddings::Embedding({1.0f, 0.0f, 0.0f}));
        }
        return embeddings;
      });

  base::test::TestFuture<void> logging_future;
  logs_uploader()->WaitForLogUpload(logging_future.GetCallback());

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kMultiSignalScoring;
  service()->GetRelevantTabsForQuery(
      options, "test query", /*explicit_urls=*/{}, future.GetCallback());

  ASSERT_TRUE(logging_future.Wait());

  optimization_guide::proto::ContextualTasksContextQuality
      uploaded_quality_log = logs_uploader()
                                 ->uploaded_logs()[0]
                                 ->contextual_tasks_context()
                                 .quality();

  ASSERT_GE(uploaded_quality_log.eligible_tabs().size(), 1);

  EXPECT_NEAR(uploaded_quality_log.query_active_tab_title_similarity(), -1.0f,
              0.001f);

  // The cosine similarity between vectors of -1.0f and 1.0f is exactly -1.0f.
  // This proves it's computing the cross-tab title similarity correctly.
  EXPECT_NEAR(
      uploaded_quality_log.eligible_tabs()[0].active_tab_title_similarity(),
      -1.0f, 0.001f);
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

  std::vector<page_content_annotations::PassageEmbedding> fake_page_embeddings =
      {// Not match.
       {std::make_pair(
            "passage 1",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})},
       // Not added - page content skipped.
       {std::make_pair(
            "passage 2",
            page_content_annotations::EmbeddingPassageType::kPageContent),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})},
       // Added - title passage matches.
       {std::make_pair("page title",
                       page_content_annotations::EmbeddingPassageType::kTitle),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillRepeatedly(Return(fake_page_embeddings));

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kMultiSignalScoring;
  service()->GetRelevantTabsForQuery(options, "some text", /*explicit_urls=*/{},
                                     future.GetCallback());
  EXPECT_EQ(1u, future.Get().size());
  EXPECT_TRUE(logs_uploader()->uploaded_logs().empty());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       SiteExclusionsFilterTabSelection) {
  test_clock_.SetNowTicks(base::TimeTicks::Now());

  // Navigates to a.test
  NavigateToValidURL();

  // Tab 2 (will become the active foreground tab)
  // Notice the case difference won't matter when filtering because domains
  // canonicalize to lowercase and the exclusions in prefs are kept lowercase.
  GURL url2 = embedded_test_server()->GetURL("B.test",
                                             "/optimization_guide/hello.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  GURL url3 = embedded_test_server()->GetURL("c.test",
                                             "/optimization_guide/hello.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url3, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  GURL url4 = embedded_test_server()->GetURL("en.c.test",
                                             "/optimization_guide/hello.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url4, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  NotifyEmbedderMetadata();

  // Initial prefs default site exclusions are empty so all tabs are eligible.
  {
    base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
        future;
    TabSelectionOptions options;
    options.tab_selection_mode = mojom::TabSelectionMode::kMultiSignalScoring;
    service()->GetRelevantTabsForQuery(options, "summarize the test page",
                                       /*explicit_urls=*/{},
                                       future.GetCallback());

    auto tabs = future.Get();
    EXPECT_EQ(4u, tabs.size());
  }

  // Add a couple of exclusions and save to prefs.
  base::Time now = base::Time::Now();
  base::DictValue site_exclusions;
  site_exclusions.Set("b.test",
                      static_cast<double>(now.InMillisecondsSinceUnixEpoch()));
  site_exclusions.Set("c.test",
                      static_cast<double>(now.InMillisecondsSinceUnixEpoch()));
  site_exclusions.Set("some.overly.specific.subdomain.a.test",
                      static_cast<double>(now.InMillisecondsSinceUnixEpoch()));
  SaveSiteExclusionsToPrefs(GetProfile()->GetPrefs(), site_exclusions);

  // Now only the initial navigation (a.test) remains since b.test, c.test,
  // and en.c.test get filtered by the above exclusions.
  {
    base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
        future;
    TabSelectionOptions options;
    options.tab_selection_mode = mojom::TabSelectionMode::kMultiSignalScoring;
    service()->GetRelevantTabsForQuery(options, "summarize the test page",
                                       /*explicit_urls=*/{},
                                       future.GetCallback());

    auto tabs = future.Get();
    EXPECT_EQ(1u, tabs.size());
    EXPECT_EQ(tabs[0]->GetLastCommittedURL().GetHost(), kValidUrlDomain);
  }
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest, SuccessWithMlModel) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // Verifies that with ML model mode selected, the service execution follows
  // the ML model execution path and returns results.
  base::HistogramTester histogram_tester;

  // unit_test_tab_relevance.tflite expects 25 float inputs.
  optimization_guide::proto::TabRelevanceModelMetadata metadata;
  metadata.set_num_features(25);
  metadata.set_num_passages_per_tab(10);
  metadata.add_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_LENGTH);
  metadata.add_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_TITLE_LEXICAL_SIMILARITY);
  metadata.add_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_ACTIVE_TAB_SIMILARITY);
  metadata.add_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_CANDIDATE_TAB_SIMILARITY);
  metadata.add_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_ACTIVE_CANDIDATE_TAB_SIMILARITY);

  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/"
      "optimization_guide.proto.TabRelevanceModelMetadata");
  metadata.SerializeToString(any_metadata.mutable_value());

  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
  base::FilePath model_file_path =
      test_data_dir.AppendASCII("components")
          .AppendASCII("test")
          .AppendASCII("data")
          .AppendASCII("contextual_tasks")
          .AppendASCII("unit_test_tab_relevance.tflite");
  {
    ASSERT_TRUE(base::PathExists(model_file_path));
  }

  auto model_info = optimization_guide::TestModelInfoBuilder()
                        .SetModelFilePath(model_file_path)
                        .SetModelMetadata(any_metadata)
                        .Build();
  UpdateModel(optimization_guide::proto::
                  OPTIMIZATION_TARGET_CONTEXTUAL_TASKS_TAB_RELEVANCE,
              *model_info);

  NavigateToValidURL();

  // Open a second tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), valid_url(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  NotifyEmbedderMetadata();

  std::vector<page_content_annotations::PassageEmbedding> fake_page_embeddings =
      {{std::make_pair("page title",
                       page_content_annotations::EmbeddingPassageType::kTitle),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillRepeatedly(Return(fake_page_embeddings));

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kStaticSignalsMlModel;
  // Set threshold lower than expected score (0.515047f for 5 words).
  options.min_model_score = 0.5f;

  service()->GetRelevantTabsForQuery(options, "summarize the test page now",
                                     /*explicit_urls=*/{},
                                     future.GetCallback());

  // Expect 2 tabs because both tabs have the same URL and deduplication is
  // disabled.
  EXPECT_EQ(2u, future.Get().size());
}

class ContextualTasksContextServiceDeduplicateTest
    : public ContextualTasksContextServiceTest {
 protected:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {kContextualTasksContext,
             {{"ContextualTasksContextDeduplicateByUrl", "true"},
              {"ContextualTasksContextOnlyUseTitles", "false"},
              {"ContextualTasksContextTabSelectionScoreThreshold", "0.8"},
              {"ContextualTasksContextContentVisibilityThreshold", "0.8"}}},
            {kContextualTasksContextLogging, {}},
        },
        /*disabled_features=*/{});
  }
};

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceDeduplicateTest,
                       DeduplicateTabs) {
  base::HistogramTester histogram_tester;

  NavigateToValidURL();

  // Open a second tab with the same URL.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), valid_url(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Open a third tab with a different URL.
  GURL url2 = embedded_test_server()->GetURL("b.test",
                                             "/optimization_guide/hello.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  NotifyEmbedderMetadata();

  std::vector<page_content_annotations::PassageEmbedding> fake_page_embeddings =
      {{std::make_pair("page title",
                       page_content_annotations::EmbeddingPassageType::kTitle),
        passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})}};
  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_))
      .WillRepeatedly(Return(fake_page_embeddings));

  base::test::TestFuture<std::vector<base::WeakPtr<content::WebContents>>>
      future;
  TabSelectionOptions options;
  options.tab_selection_mode = mojom::TabSelectionMode::kEmbeddingsMatch;
  service()->GetRelevantTabsForQuery(options, "some text", /*explicit_urls=*/{},
                                     future.GetCallback());

  // Expect 2 tabs: one for valid_url() (deduped) and one for url2.
  auto tabs = future.Get();
  EXPECT_EQ(2u, tabs.size());

  std::vector<GURL> urls;
  for (const auto& tab : tabs) {
    if (tab) {
      urls.push_back(tab->GetLastCommittedURL());
    }
  }
  EXPECT_THAT(urls, UnorderedElementsAre(valid_url(), url2));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest,
                       ModelHandlerInitialization) {
  // Verifies that the model handler is properly instantiated.
  EXPECT_TRUE(GetModelHandler() != nullptr);
}

class ContextualTasksContextServiceSmartTabSharingTest
    : public ContextualTasksContextServiceTest {
 protected:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {kContextualTasksContext,
             {{"ContextualTasksContextSmartTabSharing", "true"},
              {"ContextualTasksContextOnlyUseTitles", "false"},
              {"ContextualTasksContextTabSelectionScoreThreshold", "0.8"},
              {"ContextualTasksContextContentVisibilityThreshold", "0.8"}}},
            {kContextualTasksContextLogging, {}},
        },
        /*disabled_features=*/{});
  }
};

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceSmartTabSharingTest,
                       GetIsSmartTabSharingEnabled) {
  Profile* profile = browser()->profile();

  EXPECT_TRUE(
      ContextualTasksContextService::GetIsSmartTabSharingEnabled(profile));

  profile->GetPrefs()->SetInteger(
      kContextualTasksSmartTabSharingSettings,
      static_cast<int>(SmartTabSharingSettingsValue::kDisabled));
  EXPECT_FALSE(
      ContextualTasksContextService::GetIsSmartTabSharingEnabled(profile));

  profile->GetPrefs()->SetInteger(
      kContextualTasksSmartTabSharingSettings,
      static_cast<int>(SmartTabSharingSettingsValue::kEnabled));
  EXPECT_TRUE(
      ContextualTasksContextService::GetIsSmartTabSharingEnabled(profile));
}

}  // namespace contextual_tasks

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/passage_embeddings/page_embeddings_service.h"
#include "chrome/browser/passage_embeddings/page_embeddings_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_tasks/public/features.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
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
  MockPageEmbeddingsService(
      page_content_annotations::PageContentExtractionService*
          page_content_extraction_service)
      : PageEmbeddingsService(page_content_extraction_service) {}
  ~MockPageEmbeddingsService() override = default;

  MOCK_METHOD(std::vector<passage_embeddings::PassageEmbedding>,
              GetEmbeddings,
              (content::WebContents * web_contents),
              (const override));
};

class ContextualTasksContextServiceTest : public InProcessBrowserTest {
 public:
  ContextualTasksContextServiceTest() { InitializeFeatureList(); }

  ~ContextualTasksContextServiceTest() override {
    scoped_feature_list_.Reset();
  }

  virtual void InitializeFeatureList() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kContextualTasksContext,
          {{{"ContextualTasksContextOnlyUseTitles", "false"}}}},
         {passage_embeddings::kPassageEmbedder, {}}},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    InProcessBrowserTest::SetUpOnMainThread();

    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/optimization_guide");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* browser_context) override {
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
                [](passage_embeddings::EmbedderMetadataProvider*
                       embedder_metadata_provider,
                   passage_embeddings::Embedder* embedder,
                   content::BrowserContext* context)
                    -> std::unique_ptr<KeyedService> {
                  Profile* profile = Profile::FromBrowserContext(context);
                  return std::make_unique<ContextualTasksContextService>(
                      profile,
                      passage_embeddings::PageEmbeddingsServiceFactory::
                          GetForProfile(profile),
                      embedder_metadata_provider, embedder,
                      OptimizationGuideKeyedServiceFactory::GetForProfile(
                          profile));
                },
                &embedder_metadata_provider_, &embedder_));
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
    GURL url(embedded_test_server()->GetURL("a.test",
                                            "/optimization_guide/hello.html"));
    content::NavigateToURLBlockUntilNavigationsComplete(web_contents, url, 1);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  FakeEmbedderMetadataProvider embedder_metadata_provider_;
  FakeEmbedder embedder_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest, NoEmbedder) {
  base::HistogramTester histogram_tester;

  NavigateToValidURL();

  base::test::TestFuture<std::vector<content::WebContents*>> future;
  service()->GetRelevantTabsForQuery("some text", future.GetCallback());
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
  service()->GetRelevantTabsForQuery("some text", future.GetCallback());
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
  service()->GetRelevantTabsForQuery("some text", future.GetCallback());
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
  service()->GetRelevantTabsForQuery("some text", future.GetCallback());
  EXPECT_EQ(1u, future.Get().size());

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.RelevantTabsCount", 1, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Context.ContextCalculationLatency", 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.ContextDeterminationStatus",
      ContextDeterminationStatus::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextServiceTest, SkipsNonHttp) {
  base::HistogramTester histogram_tester;

  NotifyEmbedderMetadata();

  EXPECT_CALL(*page_embeddings_service(), GetEmbeddings(_)).Times(0);

  base::test::TestFuture<std::vector<content::WebContents*>> future;
  service()->GetRelevantTabsForQuery("some text", future.GetCallback());
  EXPECT_TRUE(future.Get().empty());

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Context.RelevantTabsCount", 0, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Context.ContextCalculationLatency", 1);
}

class ContextualTasksContextServiceTitlesOnlyTest
    : public ContextualTasksContextServiceTest {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kContextualTasksContext,
          {{{"ContextualTasksContextOnlyUseTitles", "true"}}}},
         {passage_embeddings::kPassageEmbedder, {}}},
        /*disabled_features=*/{});
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
  service()->GetRelevantTabsForQuery("some text", future.GetCallback());
  EXPECT_EQ(1u, future.Get().size());
}

}  // namespace contextual_tasks

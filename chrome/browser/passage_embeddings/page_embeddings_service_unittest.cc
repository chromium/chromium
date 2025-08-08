// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/passage_embeddings/page_embeddings_service.h"

#include <memory>

#include "base/check.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::IsEmpty;
using testing::Return;

namespace passage_embeddings {

std::vector<std::string> GenerateCandidates(
    const optimization_guide::proto::AnnotatedPageContent& page_content,
    int passages_to_generate) {
  return {page_content.main_frame_data().title()};
}

class EmbedderMock : public Embedder {
 public:
  MOCK_METHOD(TaskId,
              ComputePassagesEmbeddings,
              (PassagePriority priority,
               std::vector<std::string> passages,
               ComputePassagesEmbeddingsCallback callback),
              (override));

  MOCK_METHOD(void,
              ReprioritizeTasks,
              (PassagePriority priority, const std::set<TaskId>& tasks),
              (override));

  MOCK_METHOD(bool, TryCancel, (TaskId task_id), (override));
};

class ObserverMock : public PageEmbeddingsService::Observer {
 public:
  MOCK_METHOD(void,
              OnPageEmbeddingsAvailable,
              (content::WebContents * web_contents),
              (override));
};

class PageEmbeddingsServiceTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    page_embeddings_service_.emplace(
        base::BindRepeating(&GenerateCandidates),
        page_content_annotations::PageContentExtractionServiceFactory::
            GetForProfile(Profile::FromBrowserContext(GetBrowserContext())),
        &embedder_mock_);
  }

  void TearDown() override {
    page_embeddings_service_.reset();
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    return std::make_unique<TestingProfile>();
  }

  PageEmbeddingsService& page_embeddings_service() {
    CHECK(page_embeddings_service_.has_value());
    return *page_embeddings_service_;
  }

  EmbedderMock& embedder_mock() { return embedder_mock_; }

 private:
  EmbedderMock embedder_mock_;
  std::optional<PageEmbeddingsService> page_embeddings_service_;
};

// Validates that candidate passages are generated from AnnotatedPageContent.
TEST_F(PageEmbeddingsServiceTest, GeneratesCandidatePassages) {
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  optimization_guide::proto::AnnotatedPageContent page_content;
  page_content.mutable_main_frame_data()->set_title("passage text");

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault([](PassagePriority priority,
                        std::vector<std::string> passages,
                        Embedder::ComputePassagesEmbeddingsCallback callback) {
        EXPECT_THAT(passages, ElementsAre("passage text"));
        return 1;
      });

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings);

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(), page_content);
}

// Validates that the observer is notified on the generation of new embeddings
// for a WebContents.
TEST_F(PageEmbeddingsServiceTest, NotifiesObserver) {
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();

  ObserverMock observer;
  page_embeddings_service().AddObserver(&observer);

  Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback;

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault([&](PassagePriority priority,
                         std::vector<std::string> passages,
                         Embedder::ComputePassagesEmbeddingsCallback callback) {
        compute_passages_embeddings_callback = std::move(callback);
        return 1;
      });

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings);
  EXPECT_CALL(observer, OnPageEmbeddingsAvailable(web_contents.get()));

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  std::move(compute_passages_embeddings_callback)
      .Run({""}, {Embedding({1.0f})}, 1, ComputeEmbeddingsStatus::kSuccess);
  page_embeddings_service().RemoveObserver(&observer);
}

// Validates that the observer is not notified if the WebContents associated
// with the passages is destroyed before the embeddings could be computed.
TEST_F(PageEmbeddingsServiceTest,
       DoesntNotifyObserverIfWebContentsIsDestroyed) {
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();

  ObserverMock observer;
  page_embeddings_service().AddObserver(&observer);

  Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback;

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault([&](PassagePriority priority,
                         std::vector<std::string> passages,
                         Embedder::ComputePassagesEmbeddingsCallback callback) {
        web_contents.reset();
        compute_passages_embeddings_callback = std::move(callback);
        return 1;
      });

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings);
  EXPECT_CALL(observer, OnPageEmbeddingsAvailable(web_contents.get())).Times(0);

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  std::move(compute_passages_embeddings_callback)
      .Run({""}, {Embedding({1.0f})}, 1, ComputeEmbeddingsStatus::kSuccess);

  page_embeddings_service().RemoveObserver(&observer);
}

// Validates that embeddings can be retrieved after they are computed.
TEST_F(PageEmbeddingsServiceTest, GetEmbeddings) {
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();

  Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback;

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault([&](PassagePriority priority,
                         std::vector<std::string> passages,
                         Embedder::ComputePassagesEmbeddingsCallback callback) {
        compute_passages_embeddings_callback = std::move(callback);
        return 1;
      });

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings);

  EXPECT_THAT(page_embeddings_service().GetEmbeddings(web_contents.get()),
              IsEmpty());

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  std::move(compute_passages_embeddings_callback)
      .Run({"passage text"}, {Embedding({1.0f})}, 1,
           ComputeEmbeddingsStatus::kSuccess);

  std::vector<PassageEmbedding> embeddings =
      page_embeddings_service().GetEmbeddings(web_contents.get());
  ASSERT_EQ(1u, embeddings.size());
  EXPECT_EQ("passage text", embeddings[0].passage);
  EXPECT_THAT(embeddings[0].embedding.GetData(), ElementsAre(1.0f));
}

// Validates that embeddings can be retrieved after they are computed.
TEST_F(PageEmbeddingsServiceTest, EmbeddingsNotPresentOnError) {
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();

  Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback;

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault([&](PassagePriority priority,
                         std::vector<std::string> passages,
                         Embedder::ComputePassagesEmbeddingsCallback callback) {
        compute_passages_embeddings_callback = std::move(callback);
        return 1;
      });

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings);

  EXPECT_THAT(page_embeddings_service().GetEmbeddings(web_contents.get()),
              IsEmpty());

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  std::move(compute_passages_embeddings_callback)
      .Run({"passage text"}, {Embedding({1.0f})}, 1,
           ComputeEmbeddingsStatus::kExecutionFailure);

  std::vector<PassageEmbedding> embeddings =
      page_embeddings_service().GetEmbeddings(web_contents.get());
  EXPECT_TRUE(embeddings.empty());
}

// Validates that seeing new page contents while embeddings are still pending
// results in canceling the previous embedding computation.
TEST_F(PageEmbeddingsServiceTest, NewPageContentCancelsExistingEmbeddingTask) {
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();

  // Return the task id and don't compute the embeddings.
  ON_CALL(embedder_mock(), ComputePassagesEmbeddings).WillByDefault(Return(1));

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(2);

  EXPECT_THAT(page_embeddings_service().GetEmbeddings(web_contents.get()),
              IsEmpty());

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings).WillByDefault(Return(2));
  EXPECT_CALL(embedder_mock(), TryCancel(1));

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());
}

// Validates that the embeddings are no longer available after destroying the
// WebContents.
TEST_F(PageEmbeddingsServiceTest, EmbeddingsRemovedOnWebContentsDestruction) {
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();

  Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback;

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault([&](PassagePriority priority,
                         std::vector<std::string> passages,
                         Embedder::ComputePassagesEmbeddingsCallback callback) {
        compute_passages_embeddings_callback = std::move(callback);
        return 1;
      });

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings);

  EXPECT_THAT(page_embeddings_service().GetEmbeddings(web_contents.get()),
              IsEmpty());

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  web_contents.reset();

  std::move(compute_passages_embeddings_callback)
      .Run({""}, {Embedding({1.0f})}, 1, ComputeEmbeddingsStatus::kSuccess);

  EXPECT_TRUE(
      page_embeddings_service().GetEmbeddings(web_contents.get()).empty());
}

// Validates that the cancelled embeddings are ignored, even if received due to
// already being returned asynchronously.
TEST_F(PageEmbeddingsServiceTest, CancelledEmbeddingsAreIgnored) {
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();

  Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback1;
  Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback2;

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(2);

  EXPECT_THAT(page_embeddings_service().GetEmbeddings(web_contents.get()),
              IsEmpty());

  EXPECT_CALL(embedder_mock(), TryCancel(1));

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault([&](PassagePriority priority,
                         std::vector<std::string> passages,
                         Embedder::ComputePassagesEmbeddingsCallback callback) {
        compute_passages_embeddings_callback1 = std::move(callback);
        return 1;
      });

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault([&](PassagePriority priority,
                         std::vector<std::string> passages,
                         Embedder::ComputePassagesEmbeddingsCallback callback) {
        compute_passages_embeddings_callback2 = std::move(callback);
        return 2;
      });

  // Providing page content a second time should try to cancel the first
  // embedding computation.
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  std::move(compute_passages_embeddings_callback1)
      .Run({"passage text 1"}, {Embedding({1.0f})}, 1,
           ComputeEmbeddingsStatus::kSuccess);

  EXPECT_TRUE(
      page_embeddings_service().GetEmbeddings(web_contents.get()).empty());

  std::move(compute_passages_embeddings_callback2)
      .Run({"passage text 2"}, {Embedding({1.0f})}, 2,
           ComputeEmbeddingsStatus::kSuccess);

  std::vector<PassageEmbedding> embeddings =
      page_embeddings_service().GetEmbeddings(web_contents.get());
  ASSERT_EQ(1u, embeddings.size());
  EXPECT_EQ("passage text 2", embeddings[0].passage);
  EXPECT_THAT(embeddings[0].embedding.GetData(), ElementsAre(1.0f));
}

}  // namespace passage_embeddings

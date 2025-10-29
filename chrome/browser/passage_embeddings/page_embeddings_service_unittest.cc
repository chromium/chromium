// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/passage_embeddings/page_embeddings_service.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AnyNumber;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Return;

namespace passage_embeddings {

std::vector<std::pair<std::string, PassageType>> GenerateCandidates(
    const optimization_guide::proto::AnnotatedPageContent& page_content,
    int page_content_passages_to_generate) {
  return {std::make_pair(page_content.main_frame_data().title(),
                         PassageType::kTitle)};
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
  MOCK_METHOD(PageEmbeddingsService::Priority, GetDefaultPriority, (), (const));

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

  std::unique_ptr<content::WebContents> CreateTestWebContentsWithVisibility(
      content::Visibility visibility) {
    std::unique_ptr<content::WebContents> web_contents =
        CreateTestWebContents();
    // WebContents won't actually set visibility to a non-visible state until
    // it's first set to visible.
    if (visibility != content::Visibility::VISIBLE) {
      web_contents->UpdateWebContentsVisibility(content::Visibility::VISIBLE);
    }
    web_contents->UpdateWebContentsVisibility(visibility);
    return web_contents;
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
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);
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
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  ObserverMock observer;
  EXPECT_CALL(observer, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kDefault));
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
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  ObserverMock observer;
  EXPECT_CALL(observer, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kDefault));
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
  EXPECT_CALL(observer, OnPageEmbeddingsAvailable(web_contents.get())).Times(0);

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  web_contents.reset();

  std::move(compute_passages_embeddings_callback)
      .Run({""}, {Embedding({1.0f})}, 1, ComputeEmbeddingsStatus::kSuccess);

  page_embeddings_service().RemoveObserver(&observer);
}

// Validates that embeddings can be retrieved after they are computed.
TEST_F(PageEmbeddingsServiceTest, GetEmbeddings) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

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
  EXPECT_EQ("passage text", embeddings[0].passage.first);
  EXPECT_EQ(PassageType::kTitle, embeddings[0].passage.second);
  EXPECT_THAT(embeddings[0].embedding.GetData(), ElementsAre(1.0f));
}

// Validates that embeddings can be retrieved after they are computed.
TEST_F(PageEmbeddingsServiceTest, EmbeddingsNotPresentOnError) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

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
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

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
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

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
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

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
  EXPECT_EQ("passage text 2", embeddings[0].passage.first);
  EXPECT_EQ(PassageType::kTitle, embeddings[0].passage.second);
  EXPECT_THAT(embeddings[0].embedding.GetData(), ElementsAre(1.0f));
}

TEST_F(PageEmbeddingsServiceTest, DoesNotCrashOnCancel) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

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

  // Mimic real cancelling.
  std::move(compute_passages_embeddings_callback1)
      .Run({"passage text 1"}, {}, 1, ComputeEmbeddingsStatus::kCanceled);

  EXPECT_TRUE(
      page_embeddings_service().GetEmbeddings(web_contents.get()).empty());

  std::move(compute_passages_embeddings_callback2)
      .Run({"passage text 2"}, {Embedding({1.0f})}, 2,
           ComputeEmbeddingsStatus::kSuccess);

  std::vector<PassageEmbedding> embeddings =
      page_embeddings_service().GetEmbeddings(web_contents.get());
  ASSERT_EQ(1u, embeddings.size());
  EXPECT_EQ("passage text 2", embeddings[0].passage.first);
  EXPECT_EQ(PassageType::kTitle, embeddings[0].passage.second);
  EXPECT_THAT(embeddings[0].embedding.GetData(), ElementsAre(1.0f));
}

// Validates that the embeddings are computed with the priority of the highest
// priority observer.
TEST_F(PageEmbeddingsServiceTest, PrioritySetBasedOnHighestPriorityObserver) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  ObserverMock observer_urgent;
  EXPECT_CALL(observer_urgent, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kUrgent));

  ObserverMock observer_user_blocking;
  EXPECT_CALL(observer_user_blocking, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kUserBlocking));

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(AnyNumber());
  EXPECT_CALL(embedder_mock(), TryCancel).Times(AnyNumber());
  EXPECT_CALL(embedder_mock(), ReprioritizeTasks).Times(AnyNumber());

  const auto set_priority_expectation =
      [this](PassagePriority expected_priority) {
        ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
            .WillByDefault(
                [expected_priority](
                    PassagePriority priority, std::vector<std::string> passages,
                    Embedder::ComputePassagesEmbeddingsCallback callback) {
                  EXPECT_EQ(expected_priority, priority);
                  return 1;
                });
      };

  // With no observers the priority should be the default.
  set_priority_expectation(kPassive);
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  // Adding an urgent observer should raise the priority.
  page_embeddings_service().AddObserver(&observer_urgent);

  set_priority_expectation(kUrgent);
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  // Adding a user blocking observer should raise the priority again.
  page_embeddings_service().AddObserver(&observer_user_blocking);

  set_priority_expectation(kUserInitiated);
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  // Removing the urgent observer should not affect the priority since a higher
  // priority observer is present.
  page_embeddings_service().RemoveObserver(&observer_urgent);

  set_priority_expectation(kUserInitiated);
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  // Removing the last observer should restore the priority to the default.
  page_embeddings_service().RemoveObserver(&observer_user_blocking);

  set_priority_expectation(kPassive);
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());
}

// Validates that the embedder's tasks are reprioritized as expected.
TEST_F(PageEmbeddingsServiceTest, TasksReprioritized) {
  std::unique_ptr<content::WebContents> web_contents1 =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);
  std::unique_ptr<content::WebContents> web_contents2 =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  ObserverMock observer_urgent;
  EXPECT_CALL(observer_urgent, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kUrgent));

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(AnyNumber());

  page_embeddings_service().AddObserver(&observer_urgent);

  Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback;

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault([&](PassagePriority priority,
                         std::vector<std::string> passages,
                         Embedder::ComputePassagesEmbeddingsCallback callback) {
        compute_passages_embeddings_callback = std::move(callback);
        return 1;
      });

  page_embeddings_service().OnPageContentExtracted(
      web_contents1->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings).WillByDefault(Return(2));
  page_embeddings_service().OnPageContentExtracted(
      web_contents2->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  ObserverMock observer_user_blocking;
  EXPECT_CALL(observer_user_blocking, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kUserBlocking));

  EXPECT_CALL(embedder_mock(),
              ReprioritizeTasks(kUserInitiated, ElementsAre(1, 2)));

  page_embeddings_service().AddObserver(&observer_user_blocking);

  std::move(compute_passages_embeddings_callback)
      .Run({"passage text"}, {Embedding({1.0f})}, 1,
           ComputeEmbeddingsStatus::kExecutionFailure);

  EXPECT_CALL(embedder_mock(), ReprioritizeTasks(kUrgent, ElementsAre(2)));

  page_embeddings_service().RemoveObserver(&observer_user_blocking);

  EXPECT_CALL(embedder_mock(), ReprioritizeTasks(kPassive, ElementsAre(2)));

  page_embeddings_service().RemoveObserver(&observer_urgent);
}

// Validates that ScopedPriority raises and lowers the priority as expected.
TEST_F(PageEmbeddingsServiceTest, ScopedPriority) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  ObserverMock observer;
  EXPECT_CALL(observer, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kUrgent));

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(AnyNumber());
  EXPECT_CALL(embedder_mock(), TryCancel).Times(AnyNumber());
  EXPECT_CALL(embedder_mock(), ReprioritizeTasks).Times(AnyNumber());

  const auto set_priority_expectation =
      [this](PassagePriority expected_priority) {
        ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
            .WillByDefault(
                [expected_priority](
                    PassagePriority priority, std::vector<std::string> passages,
                    Embedder::ComputePassagesEmbeddingsCallback callback) {
                  EXPECT_EQ(expected_priority, priority);
                  return 1;
                });
      };

  // Adding the observer raises the priority to kUrgent.
  page_embeddings_service().AddObserver(&observer);

  // Establishing the ScopedPriority should further raise the priority.
  std::optional<PageEmbeddingsService::ScopedPriority> scoped_priority =
      page_embeddings_service().RaisePriority(
          &observer, PageEmbeddingsService::kUserBlocking);

  set_priority_expectation(kUserInitiated);
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  // Destroying the ScopedPriority should revert to the lower priority.
  scoped_priority.reset();
  set_priority_expectation(kUrgent);
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  page_embeddings_service().RemoveObserver(&observer);
}

// Validates that ScopedPriority doesn't affect the priority if a higher
// priority observer is present.
TEST_F(PageEmbeddingsServiceTest, ScopedPriorityWithHigherPriorityObserver) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  ObserverMock observer_default;
  EXPECT_CALL(observer_default, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kDefault));

  ObserverMock observer_user_blocking;
  EXPECT_CALL(observer_user_blocking, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kUserBlocking));

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(AnyNumber());
  EXPECT_CALL(embedder_mock(), TryCancel).Times(AnyNumber());
  EXPECT_CALL(embedder_mock(), ReprioritizeTasks).Times(AnyNumber());

  const auto set_priority_expectation =
      [this](PassagePriority expected_priority) {
        ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
            .WillByDefault(
                [expected_priority](
                    PassagePriority priority, std::vector<std::string> passages,
                    Embedder::ComputePassagesEmbeddingsCallback callback) {
                  EXPECT_EQ(expected_priority, priority);
                  return 1;
                });
      };

  page_embeddings_service().AddObserver(&observer_default);
  page_embeddings_service().AddObserver(&observer_user_blocking);

  // Establishing the ScopedPriority should not affect the priority.
  std::optional<PageEmbeddingsService::ScopedPriority> scoped_priority =
      page_embeddings_service().RaisePriority(&observer_default,
                                              PageEmbeddingsService::kUrgent);

  set_priority_expectation(kUserInitiated);
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  // Destroying the ScopedPriority should not affect the priority.
  scoped_priority.reset();
  set_priority_expectation(kUserInitiated);
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  page_embeddings_service().RemoveObserver(&observer_user_blocking);
  page_embeddings_service().RemoveObserver(&observer_default);
}

// Validates that the active tab's embeddings are not computed while visible.
TEST_F(PageEmbeddingsServiceTest, EmbeddingsForActiveTabDeferredWhileVisible) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::VISIBLE);

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(0);

  ObserverMock observer;
  EXPECT_CALL(observer, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kDefault));
  EXPECT_CALL(observer, OnPageEmbeddingsAvailable(web_contents.get())).Times(0);

  page_embeddings_service().AddObserver(&observer);

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  page_embeddings_service().RemoveObserver(&observer);
}

// Validates that the active tab's embeddings are computed on the transition
// from visible to hidden.
TEST_F(PageEmbeddingsServiceTest, EmbeddingsForActiveTabComputedOnHidden) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::VISIBLE);

  Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback;

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault([&](PassagePriority priority,
                         std::vector<std::string> passages,
                         Embedder::ComputePassagesEmbeddingsCallback callback) {
        compute_passages_embeddings_callback = std::move(callback);
        return 1;
      });
  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(1);

  ObserverMock observer;
  EXPECT_CALL(observer, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kDefault));
  EXPECT_CALL(observer, OnPageEmbeddingsAvailable(web_contents.get())).Times(1);

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  page_embeddings_service().AddObserver(&observer);

  web_contents->UpdateWebContentsVisibility(content::Visibility::HIDDEN);

  ASSERT_FALSE(compute_passages_embeddings_callback.is_null());
  std::move(compute_passages_embeddings_callback)
      .Run({"passage text"}, {Embedding({1.0f})}, 1,
           ComputeEmbeddingsStatus::kSuccess);

  page_embeddings_service().RemoveObserver(&observer);
}

// Validates that the active tab's embeddings are computed on invoking
// ProcessAllEmbeddings().
TEST_F(PageEmbeddingsServiceTest,
       EmbeddingsForActiveTabComputedOnProcessAllEmbeddings) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::VISIBLE);

  Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback;

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault([&](PassagePriority priority,
                         std::vector<std::string> passages,
                         Embedder::ComputePassagesEmbeddingsCallback callback) {
        compute_passages_embeddings_callback = std::move(callback);
        return 1;
      });
  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(1);

  ObserverMock observer;
  EXPECT_CALL(observer, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kDefault));
  EXPECT_CALL(observer, OnPageEmbeddingsAvailable(web_contents.get())).Times(1);

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      optimization_guide::proto::AnnotatedPageContent());

  page_embeddings_service().AddObserver(&observer);

  page_embeddings_service().ProcessAllEmbeddings();

  ASSERT_FALSE(compute_passages_embeddings_callback.is_null());
  std::move(compute_passages_embeddings_callback)
      .Run({"passage text"}, {Embedding({1.0f})}, 1,
           ComputeEmbeddingsStatus::kSuccess);

  page_embeddings_service().RemoveObserver(&observer);
}

}  // namespace passage_embeddings

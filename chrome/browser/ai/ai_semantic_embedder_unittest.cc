// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_semantic_embedder.h"

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "chrome/browser/ai/ai_semantic_embedder_service_launcher.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/passage_embeddings_model_metadata.pb.h"
#include "components/passage_embeddings/core/passage_embeddings_test_util.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_semantic_embedder.mojom.h"

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using EmbedFuture =
    base::test::TestFuture<blink::mojom::SemanticEmbedderResultPtr>;

// Holds submitted jobs until cancelled.
class MockEmbedder : public passage_embeddings::TestEmbedder {
 public:
  MockEmbedder() {
    ON_CALL(*this, ComputePassagesEmbeddings)
        .WillByDefault([this](passage_embeddings::PassagePriority,
                              std::vector<std::string>,
                              ComputePassagesEmbeddingsCallback callback) {
          const uint64_t job_id = next_job_id_++;
          callbacks_.emplace(job_id, std::move(callback));
          return Job(GetWeakPtr(), job_id);
        });
  }
  ~MockEmbedder() override = default;

  MOCK_METHOD(Job,
              ComputePassagesEmbeddings,
              (passage_embeddings::PassagePriority priority,
               std::vector<std::string> passages,
               ComputePassagesEmbeddingsCallback callback),
              (override));
  MOCK_METHOD(void,
              ReprioritizeJobs,
              (passage_embeddings::PassagePriority priority,
               const std::set<uint64_t>& job_ids),
              (override));

 protected:
  bool TryCancel(uint64_t job_id) override {
    auto it = callbacks_.find(job_id);
    if (it == callbacks_.end()) {
      return false;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(it->second), std::vector<std::string>(),
                       std::vector<passage_embeddings::Embedding>(), job_id,
                       passage_embeddings::ComputeEmbeddingsStatus::kCanceled));
    callbacks_.erase(it);
    return true;
  }

 private:
  uint64_t next_job_id_ = 1;
  base::flat_map<uint64_t, ComputePassagesEmbeddingsCallback> callbacks_;
};

class AISemanticEmbedderServiceLauncherForTest
    : public AISemanticEmbedderServiceLauncher {};

class AISemanticEmbedderTest : public testing::Test {
 public:
  void SetUp() override {
    launcher_ = std::make_unique<AISemanticEmbedderServiceLauncherForTest>();
    AISemanticEmbedderServiceLauncher::SetForTesting(launcher_.get());
    optimization_guide::proto::PassageEmbeddingsModelMetadata metadata;
    metadata.set_input_window_size(2048);
    metadata.set_output_size(768);
    ASSERT_TRUE(launcher_->controller()->MaybeUpdateModelInfo(
        optimization_guide::ModelInfo{
            .model_file_path = base::FilePath(FILE_PATH_LITERAL("embeddings")),
            .additional_files = {base::FilePath(FILE_PATH_LITERAL("sp"))},
            .version = 1,
            .model_metadata = optimization_guide::AnyWrapProto(metadata),
        }));
    set_ = std::make_unique<AIContextBoundObjectSet>(
        on_device_model::mojom::Priority::kForeground);
    set_->AddContextBoundObject(std::make_unique<AISemanticEmbedder>(
        *set_, embedder_, remote_.BindNewPipeAndPassReceiver()));
  }

  void TearDown() override {
    // The embedder's jobs must go before `embedder_` does.
    set_.reset();
    AISemanticEmbedderServiceLauncher::SetForTesting(nullptr);
    launcher_.reset();
  }

 protected:
  // Adds an embedder for another document to `set` and returns its remote.
  mojo::Remote<blink::mojom::AISemanticEmbedder> AddEmbedder(
      AIContextBoundObjectSet& set) {
    mojo::Remote<blink::mojom::AISemanticEmbedder> remote;
    set.AddContextBoundObject(std::make_unique<AISemanticEmbedder>(
        set, embedder_, remote.BindNewPipeAndPassReceiver()));
    return remote;
  }

  base::test::TaskEnvironment task_environment_;
  NiceMock<MockEmbedder> embedder_;
  std::unique_ptr<AISemanticEmbedderServiceLauncherForTest> launcher_;
  std::unique_ptr<AIContextBoundObjectSet> set_;
  mojo::Remote<blink::mojom::AISemanticEmbedder> remote_;
};

TEST_F(AISemanticEmbedderTest, CancelEmbedCancelsQueuedRequest) {
  EmbedFuture first;
  remote_->Embed(1, {"a"}, nullptr, first.GetCallback());
  EmbedFuture second;
  remote_->Embed(2, {"b"}, nullptr, second.GetCallback());
  remote_.FlushForTesting();
  EXPECT_FALSE(first.IsReady());
  EXPECT_FALSE(second.IsReady());

  remote_->CancelEmbed(1);
  // The cancelled request replies with null; the other one stays queued.
  EXPECT_FALSE(first.Take());
  remote_.FlushForTesting();
  EXPECT_FALSE(second.IsReady());
}

TEST_F(AISemanticEmbedderTest, DuplicateInFlightRequestIdIsBadMessage) {
  mojo::test::BadMessageObserver bad_message_observer;
  EmbedFuture first;
  remote_->Embed(7, {"a"}, nullptr, first.GetCallback());
  EmbedFuture second;
  remote_->Embed(7, {"b"}, nullptr, second.GetCallback());
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Duplicate embed request id");
}

TEST_F(AISemanticEmbedderTest, CancelledRequestIdIsTakenUntilItsReply) {
  mojo::test::BadMessageObserver bad_message_observer;

  // Once the cancelled request has replied, its id can be used again.
  EmbedFuture first;
  remote_->Embed(7, {"a"}, nullptr, first.GetCallback());
  remote_->CancelEmbed(7);
  EXPECT_FALSE(first.Take());
  EmbedFuture second;
  remote_->Embed(7, {"b"}, nullptr, second.GetCallback());
  remote_.FlushForTesting();
  EXPECT_FALSE(second.IsReady());
  EXPECT_FALSE(bad_message_observer.got_bad_message());

  // Before the reply, the cancelled request still holds its id. Destroying the
  // Job in CancelEmbed() posts the kCanceled completion (TryCancel()), and
  // BindPostTask() posts OnEmbedComplete() once more, while Embed(7) is the
  // next message on the pipe, so it arrives before the id is released.
  remote_->CancelEmbed(7);
  EmbedFuture third;
  remote_->Embed(7, {"c"}, nullptr, third.GetCallback());
  remote_.FlushForTesting();
  ASSERT_TRUE(bad_message_observer.got_bad_message());
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Duplicate embed request id");
}

TEST_F(AISemanticEmbedderTest, SetPrioritySkipsCancelledRequests) {
  // Calls the embedder directly so that SetPriority() runs while the
  // cancelled request still holds its id but has no Job. `remote` keeps the
  // pipe open; closing it would remove the embedder from `set_`.
  mojo::Remote<blink::mojom::AISemanticEmbedder> remote;
  auto owned_embedder = std::make_unique<AISemanticEmbedder>(
      *set_, embedder_, remote.BindNewPipeAndPassReceiver());
  AISemanticEmbedder* embedder = owned_embedder.get();
  set_->AddContextBoundObject(std::move(owned_embedder));

  EmbedFuture future;
  embedder->Embed(1, {"a"}, nullptr, future.GetCallback());
  embedder->CancelEmbed(1);
  EXPECT_CALL(embedder_, ReprioritizeJobs).Times(0);
  set_->SetPriority(on_device_model::mojom::Priority::kBackground);
  EXPECT_FALSE(future.Take());
}

TEST_F(AISemanticEmbedderTest, SubmitsRequestsWithSetPriority) {
  AIContextBoundObjectSet background_set(
      on_device_model::mojom::Priority::kBackground);
  mojo::Remote<blink::mojom::AISemanticEmbedder> background_remote =
      AddEmbedder(background_set);
  EXPECT_CALL(embedder_, ComputePassagesEmbeddings(
                             passage_embeddings::PassagePriority::kPassive,
                             ElementsAre("a"), _));
  EmbedFuture background_future;
  background_remote->Embed(1, {"a"}, nullptr, background_future.GetCallback());
  background_remote.FlushForTesting();

  EXPECT_CALL(embedder_,
              ComputePassagesEmbeddings(
                  passage_embeddings::PassagePriority::kUserInitiated,
                  ElementsAre("b"), _));
  EmbedFuture foreground_future;
  remote_->Embed(1, {"b"}, nullptr, foreground_future.GetCallback());
  remote_.FlushForTesting();
}

TEST_F(AISemanticEmbedderTest, SetPriorityLowersQueuedRequests) {
  EmbedFuture first;
  remote_->Embed(1, {"a"}, nullptr, first.GetCallback());
  EmbedFuture second;
  remote_->Embed(2, {"b"}, nullptr, second.GetCallback());
  remote_.FlushForTesting();

  AIContextBoundObjectSet foreground_set(
      on_device_model::mojom::Priority::kForeground);
  mojo::Remote<blink::mojom::AISemanticEmbedder> foreground_remote =
      AddEmbedder(foreground_set);
  EmbedFuture foreground;
  foreground_remote->Embed(1, {"c"}, nullptr, foreground.GetCallback());
  foreground_remote.FlushForTesting();

  // Hiding `set_`'s page lowers only its queued requests.
  EXPECT_CALL(embedder_,
              ReprioritizeJobs(passage_embeddings::PassagePriority::kPassive,
                               ElementsAre(1u)));
  EXPECT_CALL(embedder_,
              ReprioritizeJobs(passage_embeddings::PassagePriority::kPassive,
                               ElementsAre(2u)));
  set_->SetPriority(on_device_model::mojom::Priority::kBackground);
}

TEST_F(AISemanticEmbedderTest, SetPriorityRaisesQueuedRequests) {
  set_->SetPriority(on_device_model::mojom::Priority::kBackground);
  EXPECT_CALL(embedder_, ComputePassagesEmbeddings(
                             passage_embeddings::PassagePriority::kPassive,
                             ElementsAre("a"), _));
  EXPECT_CALL(embedder_, ComputePassagesEmbeddings(
                             passage_embeddings::PassagePriority::kPassive,
                             ElementsAre("b"), _));
  EmbedFuture first;
  remote_->Embed(1, {"a"}, nullptr, first.GetCallback());
  EmbedFuture second;
  remote_->Embed(2, {"b"}, nullptr, second.GetCallback());
  remote_.FlushForTesting();

  // Showing the page again raises its queued requests.
  EXPECT_CALL(
      embedder_,
      ReprioritizeJobs(passage_embeddings::PassagePriority::kUserInitiated,
                       ElementsAre(1u)));
  EXPECT_CALL(
      embedder_,
      ReprioritizeJobs(passage_embeddings::PassagePriority::kUserInitiated,
                       ElementsAre(2u)));
  set_->SetPriority(on_device_model::mojom::Priority::kForeground);
}

}  // namespace

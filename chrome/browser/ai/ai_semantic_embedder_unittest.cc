// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_semantic_embedder.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "chrome/browser/ai/ai_semantic_embedder_service_launcher.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/passage_embeddings_model_metadata.pb.h"
#include "components/passage_embeddings/core/passage_embeddings_features.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_semantic_embedder.mojom.h"

namespace {

using EmbedFuture =
    base::test::TestFuture<blink::mojom::SemanticEmbedderResultPtr>;

class AISemanticEmbedderServiceLauncherForTest
    : public AISemanticEmbedderServiceLauncher {};

// Requests go to the test launcher's shared embedder. Its model info has
// model paths but no embedding size, so the embedder never submits work: every
// request stays queued until it is cancelled, and no utility process starts.
class AISemanticEmbedderTest : public testing::Test {
 public:
  void SetUp() override {
    launcher_ = std::make_unique<AISemanticEmbedderServiceLauncherForTest>();
    AISemanticEmbedderServiceLauncher::SetForTesting(launcher_.get());
    launcher_->controller()->MaybeUpdateModelInfo(optimization_guide::ModelInfo{
        .model_file_path = base::FilePath(FILE_PATH_LITERAL("embeddings")),
        .additional_files = {base::FilePath(FILE_PATH_LITERAL("sp"))},
        .version = 1,
        .model_metadata = optimization_guide::AnyWrapProto(
            optimization_guide::proto::PassageEmbeddingsModelMetadata()),
    });
    set_ = std::make_unique<AIContextBoundObjectSet>(
        on_device_model::mojom::Priority::kForeground);
    set_->AddContextBoundObject(std::make_unique<AISemanticEmbedder>(
        *set_, remote_.BindNewPipeAndPassReceiver()));
  }

  void TearDown() override {
    // The embedder's jobs must go before the launcher's embedder does.
    set_.reset();
    AISemanticEmbedderServiceLauncher::SetForTesting(nullptr);
    launcher_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
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
      *set_, remote.BindNewPipeAndPassReceiver());
  AISemanticEmbedder* embedder = owned_embedder.get();
  set_->AddContextBoundObject(std::move(owned_embedder));

  EmbedFuture future;
  embedder->Embed(1, {"a"}, nullptr, future.GetCallback());
  embedder->CancelEmbed(1);
  set_->SetPriority(on_device_model::mojom::Priority::kBackground);
  EXPECT_FALSE(future.Take());
}

// Caps the shared embedder at two jobs. A third request is then dropped,
// unless a queued job has a lower priority than it: then the oldest
// lowest-priority queued job is dropped instead. Which request fails shows
// the priority each job has.
class AISemanticEmbedderPriorityTest : public AISemanticEmbedderTest {
 public:
  AISemanticEmbedderPriorityTest() {
    // Read when SetUp() creates the launcher.
    feature_list_.InitAndEnableFeatureWithParameters(
        passage_embeddings::kPassageEmbedder, {{"SchedulerMaxJobs", "2"}});
  }

 protected:
  // Adds an embedder for another document to `set` and returns its remote.
  mojo::Remote<blink::mojom::AISemanticEmbedder> AddEmbedder(
      AIContextBoundObjectSet& set) {
    mojo::Remote<blink::mojom::AISemanticEmbedder> remote;
    set.AddContextBoundObject(std::make_unique<AISemanticEmbedder>(
        set, remote.BindNewPipeAndPassReceiver()));
    return remote;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AISemanticEmbedderPriorityTest,
       BackgroundRequestMakesRoomForForegroundRequest) {
  AIContextBoundObjectSet background_set(
      on_device_model::mojom::Priority::kBackground);
  mojo::Remote<blink::mojom::AISemanticEmbedder> background_remote =
      AddEmbedder(background_set);
  EmbedFuture background_first;
  background_remote->Embed(1, {"a"}, nullptr, background_first.GetCallback());
  EmbedFuture background_second;
  background_remote->Embed(2, {"b"}, nullptr, background_second.GetCallback());
  // Both requests must be queued before the other pipe's request arrives.
  background_remote.FlushForTesting();

  EmbedFuture foreground;
  remote_->Embed(1, {"c"}, nullptr, foreground.GetCallback());

  // The queue is full, so one request is dropped and replies with null: the
  // oldest background request, not the new foreground one.
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return background_first.IsReady() || foreground.IsReady(); }));
  EXPECT_FALSE(foreground.IsReady());
  ASSERT_TRUE(background_first.IsReady());
  EXPECT_FALSE(background_first.Take());
  background_remote.FlushForTesting();
  EXPECT_FALSE(background_second.IsReady());
}

TEST_F(AISemanticEmbedderPriorityTest, SetPriorityLowersQueuedRequests) {
  EmbedFuture first;
  remote_->Embed(1, {"a"}, nullptr, first.GetCallback());
  EmbedFuture second;
  remote_->Embed(2, {"b"}, nullptr, second.GetCallback());
  remote_.FlushForTesting();

  // The page is hidden after its requests were queued.
  set_->SetPriority(on_device_model::mojom::Priority::kBackground);

  AIContextBoundObjectSet foreground_set(
      on_device_model::mojom::Priority::kForeground);
  mojo::Remote<blink::mojom::AISemanticEmbedder> foreground_remote =
      AddEmbedder(foreground_set);
  EmbedFuture foreground;
  foreground_remote->Embed(1, {"c"}, nullptr, foreground.GetCallback());

  ASSERT_TRUE(base::test::RunUntil(
      [&] { return first.IsReady() || foreground.IsReady(); }));
  EXPECT_FALSE(foreground.IsReady());
  ASSERT_TRUE(first.IsReady());
  EXPECT_FALSE(first.Take());
  remote_.FlushForTesting();
  EXPECT_FALSE(second.IsReady());
}

TEST_F(AISemanticEmbedderPriorityTest, SetPriorityRaisesQueuedRequests) {
  set_->SetPriority(on_device_model::mojom::Priority::kBackground);
  EmbedFuture first;
  remote_->Embed(1, {"a"}, nullptr, first.GetCallback());
  EmbedFuture second;
  remote_->Embed(2, {"b"}, nullptr, second.GetCallback());
  remote_.FlushForTesting();

  // The page is visible again after its requests were queued.
  set_->SetPriority(on_device_model::mojom::Priority::kForeground);

  AIContextBoundObjectSet foreground_set(
      on_device_model::mojom::Priority::kForeground);
  mojo::Remote<blink::mojom::AISemanticEmbedder> foreground_remote =
      AddEmbedder(foreground_set);
  EmbedFuture foreground;
  foreground_remote->Embed(1, {"c"}, nullptr, foreground.GetCallback());

  // Every queued job now has the new request's priority, so the new request
  // is the one dropped.
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return first.IsReady() || foreground.IsReady(); }));
  EXPECT_FALSE(first.IsReady());
  ASSERT_TRUE(foreground.IsReady());
  EXPECT_FALSE(foreground.Take());
  remote_.FlushForTesting();
  EXPECT_FALSE(second.IsReady());
}

}  // namespace

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_semantic_embedder.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "chrome/browser/ai/ai_semantic_embedder_service_launcher.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/passage_embeddings_model_metadata.pb.h"
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

}  // namespace

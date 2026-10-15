// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_embedding_gemma_v1_decision_model.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "chrome/browser/ai/ai_semantic_embedder_service_launcher.h"
#include "chrome/browser/ai/decision_model_schema_compiler.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::Gt;
using CreateClientError = blink::mojom::AIManagerCreateClientError;
using ResponseStatus = blink::mojom::ModelStreamingResponseStatus;
using DecideResult =
    base::expected<blink::mojom::AIDecisionModelResultPtr,
                   blink::mojom::AIDecisionModelDecideErrorPtr>;

passage_embeddings::Embedding MakeUnitEmbedding(std::vector<float> values) {
  auto normalized = passage_embeddings::Embedding::Normalize(std::move(values));
  CHECK(normalized.has_value());
  return passage_embeddings::Embedding(std::move(*normalized));
}

class FakeEmbedder : public passage_embeddings::Embedder {
 public:
  FakeEmbedder() = default;
  ~FakeEmbedder() override = default;

  // passage_embeddings::Embedder:
  Job ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority priority,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) override {
    const uint64_t job_id = next_job_id_++;
    last_priority_ = priority;
    batches_.push_back(passages);

    const bool should_fail = fail_next_call_;
    fail_next_call_ = false;
    const bool run_sync = run_synchronously_;
    run_synchronously_ = false;

    std::vector<passage_embeddings::Embedding> embeddings;
    if (!should_fail) {
      embeddings.reserve(passages.size());
      for (const auto& passage : passages) {
        embeddings.push_back(EmbedPassage(passage));
      }
    }

    const auto status =
        should_fail
            ? passage_embeddings::ComputeEmbeddingsStatus::kExecutionFailure
            : passage_embeddings::ComputeEmbeddingsStatus::kSuccess;
    if (run_sync) {
      std::move(callback).Run(std::move(passages), std::move(embeddings),
                              job_id, status);
      return Job(weak_ptr_factory_.GetWeakPtr(), job_id);
    }

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(passages),
                                  std::move(embeddings), job_id, status));
    return Job(weak_ptr_factory_.GetWeakPtr(), job_id);
  }

  base::WeakPtr<Embedder> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void ReprioritizeJobs(passage_embeddings::PassagePriority priority,
                        const std::set<uint64_t>& job_ids) override {
    last_priority_ = priority;
  }

  bool TryCancel(uint64_t job_id) override { return true; }

  void set_fail_next_call(bool fail) { fail_next_call_ = fail; }
  void set_run_synchronously(bool sync) { run_synchronously_ = sync; }
  passage_embeddings::PassagePriority last_priority() const {
    return last_priority_;
  }
  const std::vector<std::vector<std::string>>& batches() const {
    return batches_;
  }

 private:
  passage_embeddings::Embedding EmbedPassage(const std::string& passage) const {
    if (passage.starts_with("title: true |")) {
      return MakeUnitEmbedding({1.0f, 0.0f, 0.0f, 0.0f});
    }
    if (passage.starts_with("title: false |")) {
      return MakeUnitEmbedding({-1.0f, 0.0f, 0.0f, 0.0f});
    }
    if (passage.starts_with("title: bug |")) {
      return MakeUnitEmbedding({0.0f, 1.0f, 0.0f, 0.0f});
    }
    if (passage.starts_with("title: billing |")) {
      return MakeUnitEmbedding({0.0f, 0.0f, 1.0f, 0.0f});
    }
    if (passage.starts_with("title: feature |")) {
      return MakeUnitEmbedding({0.0f, 0.0f, 0.0f, 1.0f});
    }
    if (passage.find("charged me twice") != std::string::npos) {
      return MakeUnitEmbedding({1.0f, 0.0f, 1.0f, 0.0f});
    }
    return MakeUnitEmbedding({1.0f, 0.0f, 0.0f, 0.0f});
  }

  uint64_t next_job_id_ = 1;
  bool fail_next_call_ = false;
  bool run_synchronously_ = false;
  passage_embeddings::PassagePriority last_priority_ =
      passage_embeddings::PassagePriority::kUserInitiated;
  std::vector<std::vector<std::string>> batches_;
  base::WeakPtrFactory<FakeEmbedder> weak_ptr_factory_{this};
};

blink::mojom::AIDecisionModelQuestionPtr MakeQuestion(
    const std::string& id,
    blink::mojom::AIDecisionModelQuestionType type,
    std::vector<std::string> labels = {}) {
  auto question = blink::mojom::AIDecisionModelQuestion::New();
  question->id = id;
  question->prompt = "Prompt for " + id;
  question->type = type;
  for (auto& label : labels) {
    question->options.push_back(blink::mojom::AIDecisionModelOption::New(
        std::move(label), std::nullopt));
  }
  return question;
}

DecisionModelSchemaCompiler::CompiledSchema MakeSchema() {
  auto schema = DecisionModelSchemaCompiler::Compile(
      "Support tickets",
      {MakeQuestion("urgent",
                    blink::mojom::AIDecisionModelQuestionType::kBoolean),
       MakeQuestion("category",
                    blink::mojom::AIDecisionModelQuestionType::kChoice,
                    {"bug", "billing", "feature"})});
  CHECK(schema.has_value());
  return *std::move(schema);
}

class AIEmbeddingGemmaV1DecisionModelTest
    : public testing::Test,
      public blink::mojom::AIManagerCreateDecisionModelClient {
 protected:
  // blink::mojom::AIManagerCreateDecisionModelClient:
  void OnSessionCreated(mojo::PendingRemote<blink::mojom::AIDecisionModel>
                            pending_remote) override {
    remote_.Bind(std::move(pending_remote));
    create_future_.SetValue(std::nullopt);
  }
  void OnError(blink::mojom::AIManagerCreateClientError error,
               blink::mojom::QuotaErrorInfoPtr quota_error_info) override {
    quota_error_info_ = std::move(quota_error_info);
    create_future_.SetValue(error);
  }

  std::optional<CreateClientError> CreateModel(
      uint32_t max_passage_chars =
          AIEmbeddingGemmaV1DecisionModel::kDefaultMaxPassageChars,
      blink::mojom::QuotaErrorInfoPtr* quota_error_info = nullptr) {
    create_future_.Clear();
    quota_error_info_.reset();
    client_receiver_.reset();
    remote_.reset();
    context_bound_objects_.AddContextBoundObject(
        std::make_unique<AIEmbeddingGemmaV1DecisionModel>(
            context_bound_objects_, MakeSchema(), fake_embedder_,
            client_receiver_.BindNewPipeAndPassRemote(), max_passage_chars));
    std::optional<CreateClientError> error = create_future_.Take();
    if (quota_error_info) {
      *quota_error_info = std::move(quota_error_info_);
    }
    return error;
  }

  DecideResult Decide(const std::string& input) {
    base::test::TestFuture<DecideResult> future;
    remote_->Decide(input, future.GetCallback());
    return future.Take();
  }

  base::test::TaskEnvironment task_environment_;
  FakeEmbedder fake_embedder_;
  AIContextBoundObjectSet context_bound_objects_{
      on_device_model::mojom::Priority::kForeground};
  mojo::Receiver<blink::mojom::AIManagerCreateDecisionModelClient>
      client_receiver_{this};
  mojo::Remote<blink::mojom::AIDecisionModel> remote_;
  base::test::TestFuture<std::optional<CreateClientError>> create_future_;
  blink::mojom::QuotaErrorInfoPtr quota_error_info_;
};

TEST_F(AIEmbeddingGemmaV1DecisionModelTest,
       PrecomputesOptionsOnceAndBatchesQueriesPerDecide) {
  ASSERT_EQ(CreateModel(), std::nullopt);
  // 1 batch at creation containing 2 boolean options + 3 choice options = 5.
  ASSERT_EQ(fake_embedder_.batches().size(), 1u);
  EXPECT_EQ(fake_embedder_.batches()[0].size(), 5u);

  DecideResult result = Decide("The billing page charged me twice.");
  ASSERT_TRUE(result.has_value());
  // 1 batch at Decide() containing the single query passage.
  ASSERT_EQ(fake_embedder_.batches().size(), 2u);
  EXPECT_EQ(fake_embedder_.batches()[1].size(), 1u);

  ASSERT_EQ(result.value()->decisions.size(), 2u);
  const auto& urgent = result.value()->decisions[0];
  EXPECT_EQ(urgent->question_id, "urgent");
  EXPECT_EQ(urgent->top_label, "true");
  ASSERT_EQ(urgent->probabilities.size(), 2u);
  EXPECT_EQ(urgent->probabilities[0]->label, "true");
  EXPECT_THAT(urgent->probabilities[0]->probability, Gt(0.99f));

  const auto& category = result.value()->decisions[1];
  EXPECT_EQ(category->question_id, "category");
  EXPECT_EQ(category->top_label, "billing");
  EXPECT_THAT(category->confidence, Gt(0.99f));
}

TEST_F(AIEmbeddingGemmaV1DecisionModelTest, InputTooLargeFailsOnlyThatDecide) {
  ASSERT_EQ(CreateModel(/*max_passage_chars=*/150), std::nullopt);

  // Prefix is "task: classification | query: " (30 chars), so 120 chars fits
  // exactly at 150 chars and 121 chars exceeds quota.
  EXPECT_TRUE(Decide(std::string(120, 'x')).has_value());

  DecideResult result = Decide(std::string(121, 'x'));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->status, ResponseStatus::kErrorInputTooLarge);
  ASSERT_TRUE(result.error()->quota_error_info);
  EXPECT_EQ(result.error()->quota_error_info->requested, 151u);
  EXPECT_EQ(result.error()->quota_error_info->quota, 150u);

  EXPECT_TRUE(Decide("Short").has_value());
}

TEST_F(AIEmbeddingGemmaV1DecisionModelTest, SchemaTooLargeFailsCreation) {
  const DecisionModelSchemaCompiler::CompiledSchema schema = MakeSchema();
  uint32_t max_schema_chars = 0;
  for (size_t q = 0; q < schema.questions.size(); ++q) {
    for (size_t o = 0; o < schema.questions[q].options.size(); ++o) {
      max_schema_chars =
          std::max(max_schema_chars,
                   static_cast<uint32_t>(
                       DecisionModelSchemaCompiler::BuildOptionEmbeddingPassage(
                           schema, q, o)
                           .size()));
    }
  }

  blink::mojom::QuotaErrorInfoPtr quota_error_info;
  EXPECT_EQ(CreateModel(max_schema_chars - 1, &quota_error_info),
            CreateClientError::kInitialInputTooLarge);
  ASSERT_TRUE(quota_error_info);
  EXPECT_EQ(quota_error_info->requested, max_schema_chars);
  EXPECT_EQ(quota_error_info->quota, max_schema_chars - 1);
  EXPECT_TRUE(fake_embedder_.batches().empty());
  EXPECT_EQ(context_bound_objects_.GetSize(), 0u);

  // Exactly matching `max_schema_chars` succeeds.
  EXPECT_EQ(CreateModel(max_schema_chars), std::nullopt);
}

TEST_F(AIEmbeddingGemmaV1DecisionModelTest, EmbedFailureFailsCreation) {
  fake_embedder_.set_fail_next_call(true);
  EXPECT_EQ(CreateModel(), CreateClientError::kUnableToCreateSession);
  EXPECT_EQ(context_bound_objects_.GetSize(), 0u);

  // Synchronous failure inside ComputePassagesEmbeddings also cleans up safely.
  fake_embedder_.set_fail_next_call(true);
  fake_embedder_.set_run_synchronously(true);
  EXPECT_EQ(CreateModel(), CreateClientError::kUnableToCreateSession);
  EXPECT_EQ(context_bound_objects_.GetSize(), 0u);
}

TEST_F(AIEmbeddingGemmaV1DecisionModelTest,
       DestroyedDuringOptionEmbeddingNotifiesClient) {
  auto model = std::make_unique<AIEmbeddingGemmaV1DecisionModel>(
      context_bound_objects_, MakeSchema(), fake_embedder_,
      client_receiver_.BindNewPipeAndPassRemote());
  AIContextBoundObject* model_ptr = model.get();
  context_bound_objects_.AddContextBoundObject(std::move(model));
  context_bound_objects_.RemoveContextBoundObject(model_ptr);
  EXPECT_EQ(create_future_.Take(), CreateClientError::kUnableToCreateSession);
}

TEST_F(AIEmbeddingGemmaV1DecisionModelTest, FailedDecideKeepsModelUsable) {
  ASSERT_EQ(CreateModel(), std::nullopt);
  fake_embedder_.set_fail_next_call(true);
  DecideResult result = Decide("First input");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->status, ResponseStatus::kErrorGenericFailure);
  EXPECT_TRUE(remote_.is_connected());
  EXPECT_TRUE(Decide("Second input").has_value());
}

TEST_F(AIEmbeddingGemmaV1DecisionModelTest,
       DecideFailsWhenServiceDisallowedAfterCrashes) {
  ASSERT_EQ(CreateModel(), std::nullopt);
  auto* launcher = AISemanticEmbedderServiceLauncher::Get();
  launcher->OnServiceDisconnected(/*is_idle=*/false);
  launcher->OnServiceDisconnected(/*is_idle=*/false);
  launcher->OnServiceDisconnected(/*is_idle=*/false);
  ASSERT_FALSE(launcher->AllowedToLaunch());

  DecideResult result = Decide("Input after crashes");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->status, ResponseStatus::kErrorGenericFailure);

  launcher->RecordSuccessfulUse();
}

TEST_F(AIEmbeddingGemmaV1DecisionModelTest, SetPriorityUpdatesEmbedderJobs) {
  ASSERT_EQ(CreateModel(), std::nullopt);
  EXPECT_EQ(fake_embedder_.last_priority(),
            passage_embeddings::PassagePriority::kUserInitiated);

  context_bound_objects_.SetPriority(
      on_device_model::mojom::Priority::kBackground);
  EXPECT_TRUE(Decide("Background input").has_value());
  EXPECT_EQ(fake_embedder_.last_priority(),
            passage_embeddings::PassagePriority::kPassive);

  // Creating a model while backgrounded starts option embedding at `kPassive`.
  ASSERT_EQ(CreateModel(), std::nullopt);
  EXPECT_EQ(fake_embedder_.last_priority(),
            passage_embeddings::PassagePriority::kPassive);
}

TEST_F(AIEmbeddingGemmaV1DecisionModelTest, EmptyOrWhitespaceInputIsInvalid) {
  ASSERT_EQ(CreateModel(), std::nullopt);
  mojo::test::BadMessageObserver bad_message_observer;
  remote_->Decide("   ", base::DoNothing());
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(), "Invalid empty input");
  remote_.FlushForTesting();
  EXPECT_FALSE(remote_.is_connected());
  EXPECT_EQ(fake_embedder_.batches().size(), 1u);
}

TEST_F(AIEmbeddingGemmaV1DecisionModelTest, ConcurrentDecides) {
  ASSERT_EQ(CreateModel(), std::nullopt);
  base::test::TestFuture<DecideResult> first;
  base::test::TestFuture<DecideResult> second;
  remote_->Decide("First", first.GetCallback());
  remote_->Decide("Second", second.GetCallback());
  EXPECT_TRUE(first.Take().has_value());
  EXPECT_TRUE(second.Take().has_value());
}

}  // namespace

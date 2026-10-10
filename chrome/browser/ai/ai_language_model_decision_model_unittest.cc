// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_language_model_decision_model.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "chrome/browser/ai/decision_model_prompt_builder.h"
#include "chrome/browser/ai/decision_model_schema_compiler.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::Each;
using ::testing::FloatNear;
using ::testing::HasSubstr;
using ::testing::Not;
using CreateClientError = blink::mojom::AIManagerCreateClientError;
using Priority = on_device_model::mojom::Priority;
using ResponseStatus = blink::mojom::ModelStreamingResponseStatus;
using DecideResult =
    base::expected<blink::mojom::AIDecisionModelResultPtr,
                   blink::mojom::AIDecisionModelDecideErrorPtr>;

constexpr uint32_t kMaxTokens = 10240;

std::string TokenString(ml::Token token) {
  return "<" + base::NumberToString(static_cast<int>(token)) + ">";
}

// State shared by a fake session and all of its clones.
struct FakeModel {
  // Canned scores keyed by a question substring in the context (e.g.
  // "Prompt for urgent") and then by the option key (e.g. "A").
  std::map<std::string, std::map<std::string, float>> scores;
  // Contexts observed by `Score()`, in call order.
  std::vector<std::string> scored_contexts;
  // Priority of each session `Score()` ran on, in call order.
  std::vector<Priority> scored_priorities;
  // Text pieces passed to `Append()`, in call order.
  std::vector<std::string> appended_texts;
  // When true, the next `Clone()` drops the new session.
  bool fail_next_clone = false;
  // When true, `GetSizeInTokens()` returns 0 to simulate a token-count failure.
  bool fail_token_count = false;
  // When true, `Append()` holds its `ContextClient` remotes instead of calling
  // `OnComplete()` immediately. Clearing `held_append_clients` simulates an
  // in-flight append failing.
  bool hold_appends = false;
  std::vector<mojo::PendingRemote<on_device_model::mojom::ContextClient>>
      held_append_clients;
};

// Counts 1 token per character of text and 1 token per control token.
class FakeSession : public on_device_model::mojom::Session {
 public:
  FakeSession(
      FakeModel& model,
      mojo::UniqueReceiverSet<on_device_model::mojom::Session>& receivers,
      std::string context,
      Priority priority = Priority::kForeground)
      : model_(model),
        receivers_(receivers),
        context_(std::move(context)),
        priority_(priority) {}

  // on_device_model::mojom::Session:
  void Append(on_device_model::mojom::AppendOptionsPtr options,
              mojo::PendingRemote<on_device_model::mojom::ContextClient> client)
      override {
    for (const auto& piece : options->input->pieces) {
      if (piece->is_text()) {
        context_ += piece->get_text();
        model_->appended_texts.push_back(piece->get_text());
      } else if (piece->is_token()) {
        context_ += TokenString(piece->get_token());
      }
    }
    if (!client) {
      return;
    }
    if (model_->hold_appends) {
      model_->held_append_clients.push_back(std::move(client));
      return;
    }
    mojo::Remote<on_device_model::mojom::ContextClient> remote(
        std::move(client));
    remote->OnComplete(0);
  }
  void Generate(on_device_model::mojom::GenerateOptionsPtr options,
                mojo::PendingRemote<on_device_model::mojom::StreamingResponder>
                    responder) override {}
  void GetSizeInTokens(on_device_model::mojom::InputPtr input,
                       GetSizeInTokensCallback callback) override {
    if (model_->fail_token_count) {
      std::move(callback).Run(0);
      return;
    }
    uint32_t size = 0;
    for (const auto& piece : input->pieces) {
      size += piece->is_text() ? piece->get_text().size() : 1;
    }
    std::move(callback).Run(size);
  }
  // Matches the real service by appending `text` to the session's context.
  void Score(const std::string& text, ScoreCallback callback) override {
    model_->scored_contexts.push_back(context_);
    model_->scored_priorities.push_back(priority_);
    float score = 0.0f;
    for (const auto& [question, by_key] : model_->scores) {
      if (context_.find(question) != std::string::npos) {
        auto it = by_key.find(text);
        score = it == by_key.end() ? 0.0f : it->second;
        break;
      }
    }
    context_ += text;
    std::move(callback).Run(score);
  }
  // Matches the real service by inheriting the parent session's priority.
  void Clone(
      mojo::PendingReceiver<on_device_model::mojom::Session> session) override {
    if (model_->fail_next_clone) {
      model_->fail_next_clone = false;
      return;  // Drops `session`.
    }
    receivers_->Add(std::make_unique<FakeSession>(*model_, *receivers_,
                                                  context_, priority_),
                    std::move(session));
  }
  void GetProbabilitiesBlocking(
      const std::string& text,
      GetProbabilitiesBlockingCallback callback) override {
    std::move(callback).Run({});
  }
  void SetPriority(Priority priority) override { priority_ = priority; }
  void AsrStream(
      on_device_model::mojom::AsrStreamOptionsPtr options,
      mojo::PendingReceiver<on_device_model::mojom::AsrStreamInput> stream,
      mojo::PendingRemote<on_device_model::mojom::AsrStreamResponder> responder)
      override {}
  void Hint(on_device_model::mojom::HintOptionsPtr options) override {}

 private:
  raw_ref<FakeModel> model_;
  raw_ref<mojo::UniqueReceiverSet<on_device_model::mojom::Session>> receivers_;
  std::string context_;
  Priority priority_;
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

// Returns a compiled schema with one boolean question and one 3-way choice
// question.
DecisionModelSchemaCompiler::CompiledSchema MakeSchema() {
  std::vector<blink::mojom::AIDecisionModelQuestionPtr> questions;
  questions.push_back(MakeQuestion(
      "urgent", blink::mojom::AIDecisionModelQuestionType::kBoolean));
  questions.push_back(MakeQuestion(
      "category", blink::mojom::AIDecisionModelQuestionType::kChoice,
      {"bug", "billing", "feature"}));
  auto schema =
      DecisionModelSchemaCompiler::Compile("Support tickets", questions);
  CHECK(schema.has_value());
  return *std::move(schema);
}

// Returns the token count `FakeSession` computes for the system turn + largest
// question prompt + 4 control tokens (`kSystem`, `kEnd`, `kEnd`, `kModel`).
uint32_t LargestPromptSize(
    const DecisionModelSchemaCompiler::CompiledSchema& schema) {
  size_t largest_question = 0;
  for (size_t q = 0; q < schema.questions.size(); ++q) {
    largest_question = std::max(
        largest_question,
        DecisionModelPromptBuilder::BuildQuestionPrompt(schema, q).size());
  }
  return DecisionModelPromptBuilder::BuildSystemPrompt(schema).size() +
         largest_question + 4;
}

class AILanguageModelDecisionModelTest : public testing::Test {
 protected:
  // Creates a model with `MakeSchema()` and waits for initialization to
  // complete. Returns the creation error, if any.
  std::optional<CreateClientError> CreateModel(
      uint32_t max_tokens = kMaxTokens,
      blink::mojom::QuotaErrorInfoPtr* quota_error_info = nullptr) {
    base::test::TestFuture<std::optional<CreateClientError>,
                           blink::mojom::QuotaErrorInfoPtr>
        ready;
    remote_.reset();
    context_bound_objects_.AddContextBoundObject(
        AILanguageModelDecisionModel::CreateForTesting(
            context_bound_objects_, MakeSchema(),
            base::BindRepeating(
                &AILanguageModelDecisionModelTest::CreateSession,
                base::Unretained(this)),
            remote_.BindNewPipeAndPassReceiver(), ready.GetCallback(),
            max_tokens,
            base::BindRepeating(
                &AILanguageModelDecisionModelTest::OnDecideSucceeded,
                base::Unretained(this))));
    auto [error, quota] = ready.Take();
    if (quota_error_info) {
      *quota_error_info = std::move(quota);
    }
    return error;
  }

  DecideResult Decide(const std::string& input) {
    base::test::TestFuture<DecideResult> future;
    remote_->Decide(input, future.GetCallback());
    return future.Take();
  }

  void CreateSession(
      mojo::PendingReceiver<on_device_model::mojom::Session> session) {
    if (drop_new_sessions_) {
      return;
    }
    session_receivers_.Add(
        std::make_unique<FakeSession>(fake_model_, session_receivers_, ""),
        std::move(session));
  }

  void OnDecideSucceeded() { ++decides_succeeded_; }

  base::test::TaskEnvironment task_environment_;
  FakeModel fake_model_;
  mojo::UniqueReceiverSet<on_device_model::mojom::Session> session_receivers_;
  // Simulates an unavailable model by dropping new session requests.
  bool drop_new_sessions_ = false;
  int decides_succeeded_ = 0;
  AIContextBoundObjectSet context_bound_objects_{Priority::kForeground};
  mojo::Remote<blink::mojom::AIDecisionModel> remote_;
};

TEST_F(AILanguageModelDecisionModelTest, DecideNormalizesOptionScores) {
  ASSERT_EQ(CreateModel(), std::nullopt);
  // Raw `Score()` probabilities need not sum to 1; `DecodeResult()` normalizes
  // them across each question's options.
  fake_model_.scores["Prompt for urgent"] = {{"A", 0.6f}, {"B", 0.2f}};
  fake_model_.scores["Prompt for category"] = {
      {"A", 0.1f}, {"B", 0.3f}, {"C", 0.1f}};

  DecideResult result = Decide("The checkout page crashes.");
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result.value()->decisions.size(), 2u);

  const auto& urgent = result.value()->decisions[0];
  EXPECT_EQ(urgent->question_id, "urgent");
  EXPECT_EQ(urgent->top_label, "true");
  EXPECT_THAT(urgent->confidence, FloatNear(0.75f, 1e-5));

  const auto& category = result.value()->decisions[1];
  EXPECT_EQ(category->question_id, "category");
  EXPECT_EQ(category->top_label, "billing");
  EXPECT_THAT(category->confidence, FloatNear(0.6f, 1e-5));
  ASSERT_EQ(category->probabilities.size(), 3u);
  EXPECT_EQ(category->probabilities[2]->label, "feature");
  EXPECT_THAT(category->probabilities[2]->probability, FloatNear(0.2f, 1e-5));
}

TEST_F(AILanguageModelDecisionModelTest,
       EachOptionIsScoredRightAfterTheModelToken) {
  ASSERT_EQ(CreateModel(), std::nullopt);
  ASSERT_TRUE(Decide("Some input").has_value());

  // 2 options for "urgent" + 3 options for "category".
  ASSERT_EQ(fake_model_.scored_contexts.size(), 5u);
  const std::string model_token = TokenString(ml::Token::kModel);
  for (size_t i = 0; i < fake_model_.scored_contexts.size(); ++i) {
    const std::string& context = fake_model_.scored_contexts[i];
    // Each option is scored on a fresh clone ending at `kModel`, without tokens
    // from previously scored options.
    EXPECT_TRUE(context.ends_with(model_token)) << context;
    EXPECT_EQ(context.find(model_token), context.size() - model_token.size());
    EXPECT_THAT(context, HasSubstr("Input:\nSome input"));
    // Each context includes only the question being scored.
    if (i < 2) {
      EXPECT_THAT(context, HasSubstr("Prompt for urgent"));
      EXPECT_THAT(context, Not(HasSubstr("Prompt for category")));
    } else {
      EXPECT_THAT(context, HasSubstr("Prompt for category"));
      EXPECT_THAT(context, Not(HasSubstr("Prompt for urgent")));
    }
  }
}

// Verifies the exact turn structure: `<system>...<end><user>...<end><model>`.
TEST_F(AILanguageModelDecisionModelTest, ScoringContextHasOneTurnEach) {
  ASSERT_EQ(CreateModel(), std::nullopt);
  ASSERT_TRUE(Decide("Some input").has_value());

  const auto schema = MakeSchema();
  ASSERT_FALSE(fake_model_.scored_contexts.empty());
  EXPECT_EQ(fake_model_.scored_contexts[0],
            TokenString(ml::Token::kSystem) +
                DecisionModelPromptBuilder::BuildSystemPrompt(schema) +
                TokenString(ml::Token::kEnd) + TokenString(ml::Token::kUser) +
                DecisionModelPromptBuilder::BuildInputPrompt("Some input") +
                DecisionModelPromptBuilder::BuildQuestionPrompt(schema, 0) +
                TokenString(ml::Token::kEnd) + TokenString(ml::Token::kModel));
}

TEST_F(AILanguageModelDecisionModelTest, InputIsAppendedOncePerDecide) {
  ASSERT_EQ(CreateModel(), std::nullopt);
  ASSERT_TRUE(Decide("Some input").has_value());
  EXPECT_EQ(std::ranges::count_if(fake_model_.appended_texts,
                                  [](const std::string& text) {
                                    return text.starts_with("Input:\n");
                                  }),
            1);
}

TEST_F(AILanguageModelDecisionModelTest, InputTooLargeFailsOnlyThatDecide) {
  // Leave room for 50 input tokens.
  const uint32_t prompt_size = LargestPromptSize(MakeSchema());
  ASSERT_EQ(CreateModel(prompt_size +
                        AILanguageModelDecisionModel::kReservedTokens + 50),
            std::nullopt);

  // 1 (`kUser`) + 7 ("Input:\n") + 100 ('x') = 108 tokens > 50.
  DecideResult result = Decide(std::string(100, 'x'));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->status, ResponseStatus::kErrorInputTooLarge);
  ASSERT_TRUE(result.error()->quota_error_info);
  EXPECT_EQ(result.error()->quota_error_info->requested, 108u);
  EXPECT_EQ(result.error()->quota_error_info->quota, 50u);
  EXPECT_TRUE(fake_model_.scored_contexts.empty());

  EXPECT_TRUE(Decide("Short").has_value());
}

TEST_F(AILanguageModelDecisionModelTest, SchemaTooLargeFailsCreation) {
  const uint32_t prompt_size = LargestPromptSize(MakeSchema());
  // Creation fails if the schema leaves 0 tokens for user input.
  for (uint32_t limit : {prompt_size - 1, prompt_size}) {
    blink::mojom::QuotaErrorInfoPtr quota_error_info;
    EXPECT_EQ(CreateModel(limit + AILanguageModelDecisionModel::kReservedTokens,
                          &quota_error_info),
              CreateClientError::kInitialInputTooLarge);
    ASSERT_TRUE(quota_error_info);
    EXPECT_EQ(quota_error_info->requested, prompt_size + 1);
    EXPECT_EQ(quota_error_info->quota, limit);
    EXPECT_EQ(context_bound_objects_.GetSize(), 0u);
  }
  EXPECT_TRUE(fake_model_.appended_texts.empty());
}

TEST_F(AILanguageModelDecisionModelTest, SessionCreationFailureFailsCreation) {
  drop_new_sessions_ = true;
  EXPECT_EQ(CreateModel(), CreateClientError::kUnableToCreateSession);
  EXPECT_EQ(context_bound_objects_.GetSize(), 0u);
}

TEST_F(AILanguageModelDecisionModelTest, TokenCountFailureFailsCreation) {
  fake_model_.fail_token_count = true;
  EXPECT_EQ(CreateModel(), CreateClientError::kUnableToCalculateTokenSize);
  EXPECT_EQ(context_bound_objects_.GetSize(), 0u);
}

TEST_F(AILanguageModelDecisionModelTest, TokenCountFailureFailsDecide) {
  ASSERT_EQ(CreateModel(), std::nullopt);
  fake_model_.fail_token_count = true;
  DecideResult result = Decide("Some input");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->status, ResponseStatus::kErrorFailedToCountTokens);
}

TEST_F(AILanguageModelDecisionModelTest, FailedDecideKeepsModelUsable) {
  ASSERT_EQ(CreateModel(), std::nullopt);
  fake_model_.fail_next_clone = true;
  DecideResult result = Decide("First input");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->status, ResponseStatus::kErrorGenericFailure);
  EXPECT_TRUE(remote_.is_connected());
  EXPECT_TRUE(Decide("Second input").has_value());
}

// Blank input should have been rejected by the renderer and is treated as a bad
// mojo message.
TEST_F(AILanguageModelDecisionModelTest, BlankInputIsABadMessage) {
  ASSERT_EQ(CreateModel(), std::nullopt);
  mojo::test::BadMessageObserver bad_message_observer;
  remote_->Decide(" \t\n", base::DoNothing());
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Decide() input is empty");
  EXPECT_TRUE(fake_model_.scored_contexts.empty());
  EXPECT_EQ(context_bound_objects_.GetSize(), 0u);
  remote_.FlushForTesting();
  EXPECT_FALSE(remote_.is_connected());
}

TEST_F(AILanguageModelDecisionModelTest, DecidesRunOneAtATime) {
  ASSERT_EQ(CreateModel(), std::nullopt);
  base::test::TestFuture<DecideResult> first;
  base::test::TestFuture<DecideResult> second;
  remote_->Decide("First", first.GetCallback());
  remote_->Decide("Second", second.GetCallback());
  EXPECT_TRUE(first.Take().has_value());
  EXPECT_TRUE(second.Take().has_value());

  // All 5 options of the first `Decide()` are scored before the second starts.
  ASSERT_EQ(fake_model_.scored_contexts.size(), 10u);
  for (size_t i = 0; i < fake_model_.scored_contexts.size(); ++i) {
    EXPECT_THAT(fake_model_.scored_contexts[i],
                HasSubstr(i < 5 ? "Input:\nFirst" : "Input:\nSecond"));
  }
}

TEST_F(AILanguageModelDecisionModelTest, DecideUsesTheCurrentPriority) {
  ASSERT_EQ(CreateModel(), std::nullopt);
  context_bound_objects_.SetPriority(Priority::kBackground);
  fake_model_.scores["Prompt for urgent"] = {{"A", 0.6f}, {"B", 0.2f}};
  for (int i = 0; i < 2; ++i) {
    DecideResult result = Decide("Some input");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value()->decisions[0]->top_label, "true");
  }
  ASSERT_EQ(fake_model_.scored_contexts.size(), 10u);
  EXPECT_THAT(fake_model_.scored_priorities, Each(Priority::kBackground));
}

TEST_F(AILanguageModelDecisionModelTest,
       SessionLossDuringDecideFailsOnlyThatDecide) {
  ASSERT_EQ(CreateModel(), std::nullopt);
  fake_model_.hold_appends = true;
  base::test::TestFuture<DecideResult> future;
  remote_->Decide("First input", future.GetCallback());
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return !fake_model_.held_append_clients.empty(); }));

  // Simulate the service crashing mid-append.
  fake_model_.hold_appends = false;
  fake_model_.held_append_clients.clear();
  session_receivers_.Clear();
  DecideResult result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->status, ResponseStatus::kErrorGenericFailure);

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(remote_.is_connected());
  EXPECT_TRUE(Decide("Second input").has_value());
}

TEST_F(AILanguageModelDecisionModelTest, DecideFailsWhileModelIsGone) {
  ASSERT_EQ(CreateModel(), std::nullopt);
  drop_new_sessions_ = true;
  DecideResult result = Decide("First input");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->status, ResponseStatus::kErrorGenericFailure);

  task_environment_.RunUntilIdle();
  drop_new_sessions_ = false;
  EXPECT_TRUE(Decide("Second input").has_value());
}

TEST_F(AILanguageModelDecisionModelTest, ReportsOnlySuccessfulDecides) {
  ASSERT_EQ(CreateModel(), std::nullopt);
  EXPECT_EQ(decides_succeeded_, 0);
  ASSERT_TRUE(Decide("Some input").has_value());
  EXPECT_EQ(decides_succeeded_, 1);

  fake_model_.fail_next_clone = true;
  ASSERT_FALSE(Decide("Some input").has_value());
  EXPECT_EQ(decides_succeeded_, 1);
}

}  // namespace

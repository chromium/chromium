// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_laya_decision_model.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "chrome/browser/ai/decision_model_schema_compiler.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/on_device_model/public/cpp/capabilities.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::FloatNear;
using Priority = on_device_model::mojom::Priority;
using ResponseStatus = blink::mojom::ModelStreamingResponseStatus;
using DecideResult =
    base::expected<blink::mojom::AIDecisionModelResultPtr,
                   blink::mojom::AIDecisionModelDecideErrorPtr>;

blink::mojom::AIDecisionModelQuestionPtr MakeQuestion(
    std::string id,
    blink::mojom::AIDecisionModelQuestionType type,
    std::vector<std::pair<std::string, std::string>> options = {}) {
  auto question = blink::mojom::AIDecisionModelQuestion::New();
  question->id = std::move(id);
  question->prompt = "Prompt for " + question->id;
  question->type = type;
  for (auto& [label, description] : options) {
    question->options.push_back(blink::mojom::AIDecisionModelOption::New(
        std::move(label), std::move(description)));
  }
  return question;
}

DecisionModelSchemaCompiler::CompiledSchema MakeSchema() {
  std::vector<blink::mojom::AIDecisionModelQuestionPtr> questions;
  questions.push_back(MakeQuestion(
      "urgent", blink::mojom::AIDecisionModelQuestionType::kBoolean));
  questions.push_back(MakeQuestion(
      "category", blink::mojom::AIDecisionModelQuestionType::kChoice,
      {{"bug", "A crash"}, {"billing", "An invoice"}, {"feature", ""}}));
  questions.push_back(
      MakeQuestion("rating", blink::mojom::AIDecisionModelQuestionType::kScore,
                   {{"1", "bad"}, {"5", "good"}}));
  auto schema = DecisionModelSchemaCompiler::Compile(questions);
  CHECK(schema.has_value());
  return std::move(schema).value();
}

struct FakeDecisionState {
  std::vector<on_device_model::mojom::DecisionQueryPtr> queries;
  std::vector<std::string> inputs;
  std::optional<std::vector<std::vector<float>>> next_probabilities;
  bool fail_next_decide = false;
  bool hold_decides = false;
  bool drop_new_sessions = false;
  int healthy_completions = 0;
  std::vector<on_device_model::mojom::DecisionSession::DecideCallback>
      held_decide_callbacks;
};

class FakeDecisionSession : public on_device_model::mojom::DecisionSession {
 public:
  explicit FakeDecisionSession(FakeDecisionState& state) : state_(state) {}
  ~FakeDecisionSession() override = default;

  // on_device_model::mojom::DecisionSession:
  void Decide(const std::string& input, DecideCallback callback) override {
    state_->inputs.push_back(input);
    if (state_->hold_decides) {
      state_->held_decide_callbacks.push_back(std::move(callback));
      return;
    }
    if (state_->fail_next_decide) {
      state_->fail_next_decide = false;
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::vector<on_device_model::mojom::DecisionQuestionResultPtr> results;
    if (state_->next_probabilities.has_value()) {
      for (const auto& probs : *state_->next_probabilities) {
        results.push_back(
            on_device_model::mojom::DecisionQuestionResult::New(probs));
      }
      state_->next_probabilities.reset();
    } else if (!state_->queries.empty()) {
      for (const auto& q : state_->queries.back()->questions) {
        results.push_back(on_device_model::mojom::DecisionQuestionResult::New(
            std::vector<float>(q->options.size(), 1.0f / q->options.size())));
      }
    }
    std::move(callback).Run(std::move(results));
  }

 private:
  const base::raw_ref<FakeDecisionState> state_;
};

class FakeModelSolution : public optimization_guide::mojom::ModelSolution {
 public:
  FakeModelSolution(
      FakeDecisionState& state,
      mojo::UniqueReceiverSet<on_device_model::mojom::DecisionSession>&
          session_receivers)
      : state_(state), session_receivers_(session_receivers) {}
  ~FakeModelSolution() override = default;

  mojo::PendingRemote<optimization_guide::mojom::ModelSolution>
  BindNewPipeAndPassRemote() {
    receiver_.reset();
    return receiver_.BindNewPipeAndPassRemote();
  }

  // optimization_guide::mojom::ModelSolution:
  void CreateSession(
      mojo::PendingReceiver<on_device_model::mojom::Session> session,
      on_device_model::mojom::SessionParamsPtr params) override {}

  void CreateDecisionSession(
      on_device_model::mojom::DecisionQueryPtr query,
      mojo::PendingReceiver<on_device_model::mojom::DecisionSession> session)
      override {
    state_->queries.push_back(std::move(query));
    if (state_->drop_new_sessions) {
      return;
    }
    session_receivers_->Add(std::make_unique<FakeDecisionSession>(*state_),
                            std::move(session));
  }

  void ReportHealthyCompletion() override { ++state_->healthy_completions; }

  // Waits until every message already sent on the solution pipe has been
  // dispatched to this receiver.
  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  const base::raw_ref<FakeDecisionState> state_;
  const base::raw_ref<
      mojo::UniqueReceiverSet<on_device_model::mojom::DecisionSession>>
      session_receivers_;
  mojo::Receiver<optimization_guide::mojom::ModelSolution> receiver_{this};
};

class AILayaDecisionModelTest : public testing::Test {
 protected:
  AILayaDecisionModelTest() {
    auto config = optimization_guide::mojom::ModelSolutionConfig::New();
    config->feature_config = mojo_base::ProtoWrapper(
        optimization_guide::proto::OnDeviceModelExecutionFeatureConfig());
    config->model_versions = mojo_base::ProtoWrapper(
        optimization_guide::proto::OnDeviceModelVersions());
    model_client_ = std::make_unique<optimization_guide::ModelClient>(
        solution_.BindNewPipeAndPassRemote(), std::move(config),
        on_device_model::Capabilities());
  }

  void CreateModel(
      DecisionModelSchemaCompiler::CompiledSchema schema = MakeSchema()) {
    remote_.reset();
    context_bound_objects_.AddContextBoundObject(
        std::make_unique<AILayaDecisionModel>(
            context_bound_objects_, std::move(schema),
            model_client_ ? model_client_->GetWeakPtr() : nullptr,
            remote_.BindNewPipeAndPassReceiver()));
  }

  DecideResult Decide(const std::string& input) {
    base::test::TestFuture<DecideResult> future;
    remote_->Decide(input, future.GetCallback());
    return future.Take();
  }

  base::test::TaskEnvironment task_environment_;
  FakeDecisionState state_;
  mojo::UniqueReceiverSet<on_device_model::mojom::DecisionSession>
      session_receivers_;
  FakeModelSolution solution_{state_, session_receivers_};
  std::unique_ptr<optimization_guide::ModelClient> model_client_;
  AIContextBoundObjectSet context_bound_objects_{Priority::kForeground};
  mojo::Remote<blink::mojom::AIDecisionModel> remote_;
};

TEST_F(AILayaDecisionModelTest, ForwardsQueryAndDecodesOptionScores) {
  CreateModel();
  ASSERT_TRUE(
      base::test::RunUntil([&] { return state_.queries.size() == 1u; }));
  const auto& query = state_.queries[0];
  ASSERT_EQ(query->questions.size(), 3u);
  EXPECT_EQ(query->questions[0]->id, "urgent");
  EXPECT_EQ(query->questions[0]->type,
            on_device_model::mojom::DecisionQuestionType::kBoolean);
  ASSERT_EQ(query->questions[0]->options.size(), 2u);
  EXPECT_EQ(query->questions[0]->options[0]->label, "true");
  EXPECT_EQ(query->questions[0]->options[0]->description,
            "yes, the statement holds");
  EXPECT_EQ(query->questions[0]->options[1]->label, "false");
  EXPECT_EQ(query->questions[0]->options[1]->description,
            "no, the statement does not hold");

  EXPECT_EQ(query->questions[1]->id, "category");
  EXPECT_EQ(query->questions[1]->type,
            on_device_model::mojom::DecisionQuestionType::kChoice);
  ASSERT_EQ(query->questions[1]->options.size(), 3u);
  EXPECT_EQ(query->questions[1]->options[0]->label, "bug");
  EXPECT_EQ(query->questions[1]->options[0]->description, "A crash");

  EXPECT_EQ(query->questions[2]->id, "rating");
  EXPECT_EQ(query->questions[2]->type,
            on_device_model::mojom::DecisionQuestionType::kScore);
  ASSERT_EQ(query->questions[2]->options.size(), 2u);

  state_.next_probabilities = {
      {0.6f, 0.2f},
      {0.1f, 0.3f, 0.1f},
      {0.25f, 0.75f},
  };
  DecideResult result = Decide("The checkout page crashes.");
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result.value()->decisions.size(), 3u);

  const auto& urgent = result.value()->decisions[0];
  EXPECT_EQ(urgent->question_id, "urgent");
  EXPECT_EQ(urgent->top_label, "true");
  EXPECT_THAT(urgent->confidence, FloatNear(0.75f, 1e-5f));

  const auto& category = result.value()->decisions[1];
  EXPECT_EQ(category->question_id, "category");
  EXPECT_EQ(category->top_label, "billing");
  EXPECT_THAT(category->confidence, FloatNear(0.6f, 1e-5f));

  const auto& rating = result.value()->decisions[2];
  EXPECT_EQ(rating->question_id, "rating");
  EXPECT_EQ(rating->top_label, "5");
  ASSERT_TRUE(rating->score.has_value());
  EXPECT_THAT(*rating->score, FloatNear(4.0f, 1e-5f));
}

TEST_F(AILayaDecisionModelTest, ReusesSessionAcrossDecides) {
  CreateModel();
  EXPECT_TRUE(Decide("First input").has_value());
  EXPECT_TRUE(Decide("Second input").has_value());
  EXPECT_EQ(state_.queries.size(), 1u);
  ASSERT_EQ(state_.inputs.size(), 2u);
  EXPECT_EQ(state_.inputs[0], "First input");
  EXPECT_EQ(state_.inputs[1], "Second input");
}

TEST_F(AILayaDecisionModelTest, FailedDecideKeepsModelUsable) {
  CreateModel();
  state_.fail_next_decide = true;
  DecideResult result = Decide("First input");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->status, ResponseStatus::kErrorGenericFailure);
  EXPECT_TRUE(remote_.is_connected());
  EXPECT_TRUE(Decide("Second input").has_value());
}

TEST_F(AILayaDecisionModelTest,
       SessionLossDuringDecideFailsOnlyThatDecideAndReconnects) {
  CreateModel();
  state_.hold_decides = true;
  base::test::TestFuture<DecideResult> future;
  remote_->Decide("First input", future.GetCallback());
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return !state_.held_decide_callbacks.empty(); }));

  // Simulate the service crashing mid-Decide.
  state_.hold_decides = false;
  session_receivers_.Clear();
  state_.held_decide_callbacks.clear();
  DecideResult result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->status, ResponseStatus::kErrorGenericFailure);

  remote_.FlushForTesting();
  EXPECT_TRUE(remote_.is_connected());
  EXPECT_TRUE(Decide("Second input").has_value());
  EXPECT_EQ(state_.queries.size(), 2u);
}

TEST_F(AILayaDecisionModelTest, DecideFailsWhileModelIsGone) {
  state_.drop_new_sessions = true;
  CreateModel();
  DecideResult result = Decide("First input");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->status, ResponseStatus::kErrorGenericFailure);

  state_.drop_new_sessions = false;
  EXPECT_TRUE(Decide("Second input").has_value());
}

TEST_F(AILayaDecisionModelTest, DecideFailsWhenModelClientDestroyed) {
  CreateModel();
  ASSERT_TRUE(Decide("First input").has_value());

  session_receivers_.Clear();
  model_client_.reset();
  DecideResult result = Decide("Second input");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->status, ResponseStatus::kErrorGenericFailure);
}

TEST_F(AILayaDecisionModelTest, BlankInputIsABadMessage) {
  CreateModel();
  mojo::test::BadMessageObserver bad_message_observer;
  remote_->Decide(" \t\n", base::DoNothing());
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Decide() input is empty");
  EXPECT_TRUE(state_.inputs.empty());
  EXPECT_EQ(context_bound_objects_.GetSize(), 0u);
  remote_.FlushForTesting();
  EXPECT_FALSE(remote_.is_connected());
}

TEST_F(AILayaDecisionModelTest, ReportsOnlySuccessfulDecides) {
  CreateModel();
  EXPECT_EQ(state_.healthy_completions, 0);
  ASSERT_TRUE(Decide("Some input").has_value());
  ASSERT_TRUE(
      base::test::RunUntil([&] { return state_.healthy_completions == 1; }));

  state_.fail_next_decide = true;
  ASSERT_FALSE(Decide("Some input").has_value());
  // Any ReportHealthyCompletion() for the failed Decide would already have
  // been sent before its callback ran, so flushing the solution pipe is enough
  // to observe it.
  solution_.FlushForTesting();
  EXPECT_EQ(state_.healthy_completions, 1);
}

}  // namespace

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
#include "base/memory/weak_ptr.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "chrome/browser/ai/decision_model_schema_compiler.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/fake_manifest_broker.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/scenario_builder.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/core/model_execution/on_device_prefs.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/proto/manifest.pb.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::ElementsAre;
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

class AILayaDecisionModelTest : public testing::Test {
 protected:
  AILayaDecisionModelTest() {
    optimization_guide::ScenarioBuilder(fake_broker_.component_state())
        .AddBaseModel(
            "laya_en_wint8",
            optimization_guide::BaseModelRecipeArgs(
                optimization_guide::proto::BaseModelRecipe::BACKEND_TYPE_GPU,
                optimization_guide::proto::BaseModelRecipe::
                    PERFORMANCE_HINT_HIGHEST_QUALITY,
                {}, 100,
                {optimization_guide::proto::BaseModelRecipe::
                     MODEL_INTERFACE_DECISION}),
            optimization_guide::FakeBaseModelAsset::Content{}, "1.0.0.0")
        .AddUnsafeSolution("laya_en_wint8", "laya_en_wint8")
        .Finish();
    fake_broker_.settings().performance_class =
        on_device_model::mojom::PerformanceClass::kHigh;
    fake_broker_.Startup();

    fake_broker_.client().RequestAssetsFor("laya_en_wint8");
    base::test::TestFuture<base::WeakPtr<optimization_guide::ModelClient>>
        client_future;
    fake_broker_.client()
        .GetSubscriber("laya_en_wint8")
        .WaitForClient(client_future.GetCallback());
    model_client_ = client_future.Take();
    CHECK(model_client_);
  }

  void CreateModel(
      DecisionModelSchemaCompiler::CompiledSchema schema = MakeSchema()) {
    remote_.reset();
    context_bound_objects_.AddContextBoundObject(
        std::make_unique<AILayaDecisionModel>(
            context_bound_objects_, std::move(schema), model_client_,
            remote_.BindNewPipeAndPassReceiver()));
  }

  DecideResult Decide(const std::string& input) {
    base::test::TestFuture<DecideResult> future;
    remote_->Decide(input, future.GetCallback());
    return future.Take();
  }

  on_device_model::FakeDecisionModel* decision_model() {
    auto* service = fake_broker_.launcher().service();
    return service ? service->decision_model() : nullptr;
  }

  optimization_guide::ScopedModelBrokerFeatureList model_broker_feature_list_;
  base::test::TaskEnvironment task_environment_;
  optimization_guide::FakeManifestBroker fake_broker_;
  base::WeakPtr<optimization_guide::ModelClient> model_client_;
  AIContextBoundObjectSet context_bound_objects_{Priority::kForeground};
  mojo::Remote<blink::mojom::AIDecisionModel> remote_;
};

TEST_F(AILayaDecisionModelTest, ForwardsQueryAndDecodesOptionScores) {
  CreateModel();
  ASSERT_TRUE(base::test::RunUntil([&] {
    return decision_model() && decision_model()->queries().size() == 1u;
  }));
  const auto& query = decision_model()->queries()[0];
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

  // FakeDecisionModel assigns 0.8 probability to matching option labels and
  // splits 0.2 across non-matching options.
  DecideResult result = Decide("true billing 5");
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result.value()->decisions.size(), 3u);

  const auto& urgent = result.value()->decisions[0];
  EXPECT_EQ(urgent->question_id, "urgent");
  EXPECT_EQ(urgent->top_label, "true");
  EXPECT_THAT(urgent->confidence, FloatNear(0.8f, 1e-5f));

  const auto& category = result.value()->decisions[1];
  EXPECT_EQ(category->question_id, "category");
  EXPECT_EQ(category->top_label, "billing");
  EXPECT_THAT(category->confidence, FloatNear(0.8f, 1e-5f));

  const auto& rating = result.value()->decisions[2];
  EXPECT_EQ(rating->question_id, "rating");
  EXPECT_EQ(rating->top_label, "5");
  ASSERT_TRUE(rating->score.has_value());
  EXPECT_THAT(*rating->score, FloatNear(4.2f, 1e-5f));
}

TEST_F(AILayaDecisionModelTest, ReusesSessionAcrossDecides) {
  CreateModel();
  EXPECT_TRUE(Decide("First input").has_value());
  EXPECT_TRUE(Decide("Second input").has_value());
  ASSERT_TRUE(decision_model());
  EXPECT_EQ(decision_model()->queries().size(), 1u);
  EXPECT_THAT(decision_model()->inputs(),
              ElementsAre("First input", "Second input"));
}

TEST_F(AILayaDecisionModelTest, FailedDecideKeepsModelUsable) {
  CreateModel();
  DecideResult result = Decide("Please fail");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->status, ResponseStatus::kErrorGenericFailure);
  EXPECT_TRUE(remote_.is_connected());
  EXPECT_TRUE(Decide("Second input").has_value());
}

TEST_F(AILayaDecisionModelTest,
       SessionLossDuringDecideFailsOnlyThatDecideAndReconnects) {
  CreateModel();
  fake_broker_.settings().set_execute_delay(base::Seconds(1));
  base::test::TestFuture<DecideResult> future;
  remote_->Decide("First input", future.GetCallback());
  ASSERT_TRUE(base::test::RunUntil([&] {
    return decision_model() && decision_model()->inputs().size() == 1u;
  }));

  // Simulate the service crashing mid-Decide.
  fake_broker_.settings().set_execute_delay(base::TimeDelta());
  fake_broker_.launcher().CrashService();
  DecideResult result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->status, ResponseStatus::kErrorGenericFailure);

  remote_.FlushForTesting();
  EXPECT_TRUE(remote_.is_connected());
  EXPECT_TRUE(Decide("Second input").has_value());
  ASSERT_TRUE(decision_model());
  EXPECT_EQ(decision_model()->queries().size(), 1u);
}

TEST_F(AILayaDecisionModelTest, DecideFailsWhileModelIsGone) {
  fake_broker_.settings().set_drop_connection_request(
      on_device_model::ModelDisconnectReason::kUnspecified);
  CreateModel();
  DecideResult result = Decide("First input");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->status, ResponseStatus::kErrorGenericFailure);

  fake_broker_.settings().set_drop_connection_request(std::nullopt);
  EXPECT_TRUE(Decide("Second input").has_value());
}

TEST_F(AILayaDecisionModelTest, DecideFailsWhenModelClientDestroyed) {
  CreateModel();
  ASSERT_TRUE(Decide("First input").has_value());

  fake_broker_.SimulateShutdown();
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
  EXPECT_TRUE(!decision_model() || decision_model()->inputs().empty());
  EXPECT_EQ(context_bound_objects_.GetSize(), 0u);
  remote_.FlushForTesting();
  EXPECT_FALSE(remote_.is_connected());
}

TEST_F(AILayaDecisionModelTest, ReportsOnlySuccessfulDecides) {
  CreateModel();
  fake_broker_.local_state().SetInteger(
      optimization_guide::model_execution::prefs::localstate::
          kOnDeviceModelCrashCount,
      1);

  ASSERT_FALSE(Decide("Please fail").has_value());
  // Creating a new model issues CreateDecisionSession() on the same
  // ModelSolution pipe, flushing any prior ReportHealthyCompletion() calls.
  CreateModel();
  ASSERT_TRUE(base::test::RunUntil([&] {
    return decision_model() && decision_model()->queries().size() == 2u;
  }));
  EXPECT_EQ(fake_broker_.local_state().GetInteger(
                optimization_guide::model_execution::prefs::localstate::
                    kOnDeviceModelCrashCount),
            1);

  ASSERT_TRUE(Decide("Some input").has_value());
  ASSERT_TRUE(base::test::RunUntil([&] {
    return fake_broker_.local_state().GetInteger(
               optimization_guide::model_execution::prefs::localstate::
                   kOnDeviceModelCrashCount) == 0;
  }));
}

}  // namespace

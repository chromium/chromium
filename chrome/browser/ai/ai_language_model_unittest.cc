// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_language_model.h"

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "chrome/browser/ai/features.h"
#include "chrome/browser/component_updater/optimization_guide_on_device_model_installer.h"
#include "components/on_device_ai/ai_utils.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/mock_download_progress_observer.h"
#include "components/optimization_guide/core/model_execution/test/mock_on_device_capability.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/descriptors.pb.h"
#include "components/optimization_guide/proto/features/prompt_api.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "components/update_client/update_client.h"
#include "content/public/browser/render_widget_host_view.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "services/on_device_model/public/cpp/capabilities.h"
#include "services/on_device_model/public/cpp/features.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-shared.h"

namespace {

using ::base::test::TestFuture;
using ::on_device_model::mojom::PerformanceClass;
using ::optimization_guide::FieldSubstitution;
using ::optimization_guide::ForbidUnsafe;
using ::optimization_guide::MockDownloadProgressObserver;
using ::optimization_guide::StringValueField;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Return;
using Role = ::blink::mojom::AILanguageModelPromptRole;

constexpr uint32_t kTestMaxContextToken = 10u;
constexpr uint32_t kTestDefaultTopK = 1u;
constexpr float kTestDefaultTemperature = 0.0f;
constexpr uint32_t kTestMaxTopK = 50u;
constexpr float kTestMaxTemperature = 1.5;
constexpr uint32_t kTestMaxTokens = 100u;
constexpr uint32_t kTestConfiguredMaxOutputTokens = 10u;
static_assert(kTestDefaultTopK <= kTestMaxTopK);
static_assert(kTestDefaultTemperature <= kTestMaxTemperature);

MATCHER_P2(IsPromptWithParams, expected_top_k, expected_temp, "") {
  int top_k;
  double temp;
  static const base::NoDestructor<re2::RE2> re("TopK: (\\d+), Temp: ([\\d.]+)");
  if (re2::RE2::FullMatch(arg, *re, &top_k, &temp)) {
    return top_k == expected_top_k && std::abs(temp - expected_temp) < 0.001;
  }
  return false;
}

struct Result {
  mojo::PendingRemote<blink::mojom::AILanguageModel> language_model;
  blink::mojom::AILanguageModelInstanceInfoPtr info;
};

struct Error {
  blink::mojom::AIManagerCreateClientError error;
  blink::mojom::QuotaErrorInfoPtr quota_error_info;
};

class TestCreateLanguageModelClient
    : public blink::mojom::AIManagerCreateLanguageModelClient {
 public:
  TestCreateLanguageModelClient() = default;
  ~TestCreateLanguageModelClient() override = default;
  TestCreateLanguageModelClient(const TestCreateLanguageModelClient&) = delete;
  TestCreateLanguageModelClient& operator=(
      const TestCreateLanguageModelClient&) = delete;

  mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnResult(
      mojo::PendingRemote<blink::mojom::AILanguageModel> language_model,
      blink::mojom::AILanguageModelInstanceInfoPtr info) override {
    result_.SetValue(Result{std::move(language_model), std::move(info)});
  }

  void OnError(blink::mojom::AIManagerCreateClientError error,
               blink::mojom::QuotaErrorInfoPtr quota_error_info) override {
    result_.SetValue(
        base::unexpected(Error{error, std::move(quota_error_info)}));
  }

  TestFuture<base::expected<Result, Error>>& result() { return result_; }

 private:
  TestFuture<base::expected<Result, Error>> result_;
  mojo::Receiver<blink::mojom::AIManagerCreateLanguageModelClient> receiver_{
      this};
};

SkBitmap CreateTestBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorRED);
  return bitmap;
}

on_device_model::mojom::AudioDataPtr CreateTestAudio() {
  return on_device_model::mojom::AudioData::New();
}

// Convert a single AILanguageModelPromptContentPtr to a vector.
std::vector<blink::mojom::AILanguageModelPromptContentPtr> ToVector(
    blink::mojom::AILanguageModelPromptContentPtr content) {
  std::vector<blink::mojom::AILanguageModelPromptContentPtr> vector;
  vector.push_back(std::move(content));
  return vector;
}

// Convert a list of strings to a AILanguageModelPromptContentPtr vector.
std::vector<blink::mojom::AILanguageModelPromptContentPtr> ToContentVector(
    std::initializer_list<std::string> texts) {
  std::vector<blink::mojom::AILanguageModelPromptContentPtr> vector;
  for (const std::string& text : texts) {
    vector.push_back(blink::mojom::AILanguageModelPromptContent::NewText(text));
  }
  return vector;
}

optimization_guide::proto::FeatureTextSafetyConfiguration CreateSafetyConfig() {
  optimization_guide::proto::FeatureTextSafetyConfiguration safety_config;
  safety_config.set_feature(
      optimization_guide::proto::MODEL_EXECUTION_FEATURE_PROMPT_API);
  safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());

  auto* check = safety_config.add_request_check();
  check->mutable_input_template()->Add(
      FieldSubstitution("%s", StringValueField()));

  return safety_config;
}

// Build a mojo prompt struct with the specified `role` and `text`
blink::mojom::AILanguageModelPromptPtr MakePrompt(Role role,
                                                  const std::string& text,
                                                  bool is_prefix = false) {
  return blink::mojom::AILanguageModelPrompt::New(role, ToContentVector({text}),
                                                  is_prefix);
}

// Build a vector with a single prompt that has multiple user text contents.
std::vector<blink::mojom::AILanguageModelPromptPtr> MakeInput(
    std::initializer_list<std::string> texts) {
  std::vector<blink::mojom::AILanguageModelPromptPtr> prompts;
  prompts.push_back(blink::mojom::AILanguageModelPrompt::New(
      Role::kUser, ToContentVector(std::move(texts)), /*is_prefix=*/false));
  return prompts;
}

// Build a vector with a single prompt that has a single user text content.
std::vector<blink::mojom::AILanguageModelPromptPtr> MakeInput(
    const std::string& text) {
  return MakeInput({text});
}

// Construct a ContextItem with system prompt text.
AILanguageModel::Context::ContextItem SimpleContextItem(std::string text,
                                                        uint32_t size) {
  auto item = AILanguageModel::Context::ContextItem();
  item.tokens = size;
  item.input = on_device_model::mojom::Input::New();
  item.input->pieces = {ml::Token::kSystem, text};
  return item;
}

// Convert a ml::Token to a string for expectation matching.
const char* FormatToken(ml::Token token) {
  switch (token) {
    case ml::Token::kSystem:
      return "S: ";
    case ml::Token::kUser:
      return "U: ";
    case ml::Token::kModel:
      return "M: ";
    default:
      NOTREACHED();
  }
}

// Convert an Input to a string for expectation matching.
std::string FormatInput(const on_device_model::mojom::Input& input) {
  std::string str;
  for (const auto& piece : input.pieces) {
    if (std::holds_alternative<ml::Token>(piece)) {
      str += FormatToken(std::get<ml::Token>(piece));
    } else if (std::holds_alternative<std::string>(piece)) {
      str += std::get<std::string>(piece);
    } else if (std::holds_alternative<SkBitmap>(piece)) {
      str += "<image>";
    } else if (std::holds_alternative<ml::AudioBuffer>(piece)) {
      str += "<audio>";
    }
  }
  return str;
}

// Convert a Context to string for expectation matching.
std::string GetContextString(AILanguageModel::Context& ctx) {
  return FormatInput(*ctx.GetNonInitialPrompts());
}

class AILanguageModelTest : public AITestUtils::AITestBase {
 public:
  AILanguageModelTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kAIPromptAPIMultimodalInput, {}},
         {features::kAILanguageModelOverrideConfiguration,
          {{"ai_language_model_output_buffer", "100"}}},
         {features::kAILanguageModelAppendOutputTokensToContext, {}},
         {optimization_guide::features::kOptimizationGuideOnDeviceModel, {}},
         {optimization_guide::features::kAIModelUnloadableProgress,
          {{"ai_model_unloadable_progress_bytes", "0"}}}},
        {});
  }

 protected:
  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig CreateConfig()
      override {
    optimization_guide::proto::OnDeviceModelExecutionFeatureConfig config;
    config.set_can_skip_text_safety(true);
    optimization_guide::proto::SamplingParams default_sampling_params;
    default_sampling_params.set_top_k(kTestDefaultTopK);
    default_sampling_params.set_temperature(kTestDefaultTemperature);
    *config.mutable_sampling_params() = default_sampling_params;

    config.mutable_input_config()->set_max_context_tokens(kTestMaxTokens);

    optimization_guide::proto::PromptApiMetadata metadata;
    optimization_guide::proto::SamplingParams max_sampling_params;
    max_sampling_params.set_top_k(kTestMaxTopK);
    max_sampling_params.set_temperature(kTestMaxTemperature);
    *metadata.mutable_max_sampling_params() = max_sampling_params;
    *config.mutable_feature_metadata() =
        optimization_guide::AnyWrapProto(metadata);

    config.set_feature(optimization_guide::proto::ModelExecutionFeature::
                           MODEL_EXECUTION_FEATURE_PROMPT_API);
    return config;
  }

  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig
  CreateSafeConfig() {
    auto config = CreateConfig();
    config.set_can_skip_text_safety(false);
    return config;
  }

  mojo::Remote<blink::mojom::AILanguageModel> CreateSession(
      blink::mojom::AILanguageModelCreateOptionsPtr options =
          blink::mojom::AILanguageModelCreateOptions::New()) {
    TestCreateLanguageModelClient language_model_client;
    GetAIManagerRemote()->CreateLanguageModel(
        language_model_client.BindNewPipeAndPassRemote(), std::move(options));

    auto result = language_model_client.result().Take();
    EXPECT_OK(result);
    return mojo::Remote<blink::mojom::AILanguageModel>(
        std::move(result.value().language_model));
  }

  std::vector<std::string> Prompt(
      blink::mojom::AILanguageModel& model,
      std::vector<blink::mojom::AILanguageModelPromptPtr> input,
      on_device_model::mojom::ResponseConstraintPtr constraint = nullptr) {
    AITestUtils::TestStreamingResponder responder;
    model.Prompt(std::move(input), std::move(constraint),
                 responder.BindRemote());
    EXPECT_TRUE(responder.WaitForCompletion());
    return responder.responses();
  }

  void Append(blink::mojom::AILanguageModel& model,
              std::vector<blink::mojom::AILanguageModelPromptPtr> input) {
    AITestUtils::TestStreamingResponder responder;
    model.Append(std::move(input), responder.BindRemote());
    EXPECT_TRUE(responder.WaitForCompletion());
  }

  mojo::Remote<blink::mojom::AILanguageModel> Fork(
      blink::mojom::AILanguageModel& model) {
    TestCreateLanguageModelClient language_model_client;
    model.Fork(language_model_client.BindNewPipeAndPassRemote());

    auto result = language_model_client.result().Take();
    EXPECT_OK(result);
    return mojo::Remote<blink::mojom::AILanguageModel>(
        std::move(result.value().language_model));
  }

  void EnsureModelIsReady() {
    blink::mojom::AILanguageModelCreateOptionsPtr options =
        blink::mojom::AILanguageModelCreateOptions::New();

    TestCreateLanguageModelClient language_model_client;
    GetAIManagerRemote()->CreateLanguageModel(
        language_model_client.BindNewPipeAndPassRemote(), std::move(options));

    auto result = language_model_client.result().Take();
    EXPECT_OK(result);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests the `AIContextBoundObjectSet`'s behavior of managing the lifetime of
// `AILanguageModel`s.
TEST_F(AILanguageModelTest, AIContextBoundObjectSet) {
  mojo::Remote<blink::mojom::AIManager> mock_remote = GetAIManagerRemote();
  // Initially the `AIContextBoundObjectSet` is empty.
  ASSERT_EQ(0u, GetAIManagerContextBoundObjectSetSize());

  // After creating one `AILanguageModel`, the `AIContextBoundObjectSet`
  // contains 1 element.
  auto mock_session = CreateSession();
  ASSERT_EQ(1u, GetAIManagerContextBoundObjectSetSize());

  // After resetting the session, the size of `AIContextBoundObjectSet` becomes
  // empty again.
  mock_session.reset();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return GetAIManagerContextBoundObjectSetSize() == 0u; }));
}

TEST_F(AILanguageModelTest, Prompt) {
  auto session = CreateSession();
  EXPECT_THAT(Prompt(*session, MakeInput("foo")), ElementsAreArray({"UfooEM"}));
}

TEST_F(AILanguageModelTest, PromptTelemetry) {
  base::HistogramTester histogram_tester;
  auto session = CreateSession();
  Prompt(*session, MakeInput("foo"));

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution."
      "OnDeviceFirstResponseTime.PromptApi",
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution."
      "OnDeviceResponseCompleteTime.PromptApi",
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution."
      "OnDeviceResponseCompleteTokens.PromptApi",
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution."
      "OnDeviceResponseTokensTimeToNextToken.PromptApi",
      1);
}

TEST_F(AILanguageModelTest, MultiplePrompts) {
  auto session = CreateSession();
  EXPECT_THAT(Prompt(*session, MakeInput("foo")), ElementsAreArray({"UfooEM"}));
  EXPECT_THAT(Prompt(*session, MakeInput("bar")),
              ElementsAreArray({"UfooEM", "UbarEM"}));
  EXPECT_THAT(Prompt(*session, MakeInput("baz")),
              ElementsAreArray({"UfooEM", "UbarEM", "UbazEM"}));
}

TEST_F(AILanguageModelTest, PromptMultipleContents) {
  auto session = CreateSession();
  EXPECT_THAT(Prompt(*session, MakeInput({"foo", "bar"})),
              ElementsAreArray({"UfoobarEM"}));
}

TEST_F(AILanguageModelTest, Append) {
  auto session = CreateSession();
  Append(*session, MakeInput("foo"));
  EXPECT_THAT(Prompt(*session, MakeInput("bar")),
              ElementsAre("UfooE", "UbarEM"));
}

TEST_F(AILanguageModelTest, AppendMultipleContents) {
  auto session = CreateSession();
  Append(*session, MakeInput({"foo", "bar"}));
  EXPECT_THAT(Prompt(*session, MakeInput("baz")),
              ElementsAre("UfoobarE", "UbazEM"));
}

TEST_F(AILanguageModelTest, AppendDoesNotLogDestroyedMetric) {
  base::HistogramTester histogram_tester;
  auto session = CreateSession();
  Append(*session, MakeInput("foo"));

  session.reset();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution."
      "OnDeviceDestroyedWhileWaitingForResponseTime.PromptApi",
      0);
}

TEST_F(AILanguageModelTest, PromptTokenCounts) {
  fake_broker_->settings().set_execute_result({"hi"});
  auto session = CreateSession();

  std::string expected_tokens = "UfooEMhiE";
  {
    AITestUtils::TestStreamingResponder responder;
    session->Prompt(MakeInput("foo"), nullptr, responder.BindRemote());
    EXPECT_TRUE(responder.WaitForCompletion());
    EXPECT_EQ(responder.current_tokens(), expected_tokens.size());
  }
  expected_tokens += "UbarEMhiE";
  {
    AITestUtils::TestStreamingResponder responder;
    session->Prompt(MakeInput("bar"), nullptr, responder.BindRemote());
    EXPECT_TRUE(responder.WaitForCompletion());
    EXPECT_EQ(responder.current_tokens(), expected_tokens.size());
  }
  auto fork = Fork(*session);
  expected_tokens += "UbazEMhiE";
  {
    AITestUtils::TestStreamingResponder responder;
    fork->Prompt(MakeInput("baz"), nullptr, responder.BindRemote());
    EXPECT_TRUE(responder.WaitForCompletion());
    EXPECT_EQ(responder.current_tokens(), expected_tokens.size());
  }
}

TEST_F(AILanguageModelTest, AppendTokenCounts) {
  auto session = CreateSession();

  std::string expected_tokens = "UfooE";
  {
    AITestUtils::TestStreamingResponder responder;
    session->Append(MakeInput("foo"), responder.BindRemote());
    EXPECT_TRUE(responder.WaitForCompletion());
    EXPECT_EQ(responder.current_tokens(), expected_tokens.size());
  }
  expected_tokens += "UbarE";
  {
    AITestUtils::TestStreamingResponder responder;
    session->Append(MakeInput("bar"), responder.BindRemote());
    EXPECT_TRUE(responder.WaitForCompletion());
    EXPECT_EQ(responder.current_tokens(), expected_tokens.size());
  }
  auto fork = Fork(*session);
  expected_tokens += "UbazE";
  {
    AITestUtils::TestStreamingResponder responder;
    fork->Append(MakeInput("baz"), responder.BindRemote());
    EXPECT_TRUE(responder.WaitForCompletion());
    EXPECT_EQ(responder.current_tokens(), expected_tokens.size());
  }
}

TEST_F(AILanguageModelTest, Roles) {
  auto session = CreateSession();
  std::vector<blink::mojom::AILanguageModelPromptPtr> prompts;
  prompts.push_back(MakePrompt(Role::kUser, "user"));
  prompts.push_back(MakePrompt(Role::kSystem, "system"));
  prompts.push_back(MakePrompt(Role::kAssistant, "model"));
  EXPECT_THAT(Prompt(*session, std::move(prompts)),
              ElementsAreArray({"UuserESsystemEMmodelEM"}));
}

TEST_F(AILanguageModelTest, Fork) {
  auto session = CreateSession();
  auto fork1 = Fork(*session);

  EXPECT_THAT(Prompt(*session, MakeInput("foo")), ElementsAreArray({"UfooEM"}));
  auto fork2 = Fork(*session);

  EXPECT_THAT(Prompt(*session, MakeInput("bar")),
              ElementsAreArray({"UfooEM", "UbarEM"}));
  auto fork3 = Fork(*session);

  EXPECT_THAT(Prompt(*fork1, MakeInput("fork")), ElementsAreArray({"UforkEM"}));
  EXPECT_THAT(Prompt(*fork2, MakeInput("fork")),
              ElementsAreArray({"UfooEM", "UforkEM"}));
  auto fork4 = Fork(*fork2);
  EXPECT_THAT(Prompt(*fork3, MakeInput("fork")),
              ElementsAreArray({"UfooEM", "UbarEM", "UforkEM"}));
  EXPECT_THAT(Prompt(*session, MakeInput("baz")),
              ElementsAreArray({"UfooEM", "UbarEM", "UbazEM"}));

  EXPECT_THAT(Prompt(*fork4, MakeInput("more")),
              ElementsAreArray({"UfooEM", "UforkEM", "UmoreEM"}));
}

TEST_F(AILanguageModelTest, SamplingParams) {
  auto sampling_params = blink::mojom::AILanguageModelSamplingParams::New();
  sampling_params->top_k = 2;
  sampling_params->temperature = 1.0;

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->sampling_params = std::move(sampling_params);
  auto session = CreateSession(std::move(options));
  auto fork = Fork(*session);

  EXPECT_THAT(Prompt(*session, MakeInput("foo")),
              ElementsAre("UfooEM", "TopK: 2, Temp: 1"));
  EXPECT_THAT(Prompt(*fork, MakeInput("bar")),
              ElementsAre("UbarEM", "TopK: 2, Temp: 1"));
}

TEST_F(AILanguageModelTest, SamplingModeMappings) {
  // Test most-predictable (uses default values). Default values are omitted
  // from the output by the fake API in
  // services/on_device_model/fake/fake_chrome_ml_api.cc
  {
    auto options = blink::mojom::AILanguageModelCreateOptions::New();
    options->sampling_mode =
        blink::mojom::AILanguageModelSamplingMode::kMostPredictable;
    auto session = CreateSession(std::move(options));
    EXPECT_THAT(Prompt(*session, MakeInput("foo")), ElementsAre("UfooEM"));
  }
  // Test predictable
  {
    auto options = blink::mojom::AILanguageModelCreateOptions::New();
    options->sampling_mode =
        blink::mojom::AILanguageModelSamplingMode::kPredictable;
    auto session = CreateSession(std::move(options));
    EXPECT_THAT(Prompt(*session, MakeInput("foo")),
                ElementsAre("UfooEM", IsPromptWithParams(2, 0.2)));
  }
  // Test balanced
  {
    auto options = blink::mojom::AILanguageModelCreateOptions::New();
    options->sampling_mode =
        blink::mojom::AILanguageModelSamplingMode::kBalanced;
    auto session = CreateSession(std::move(options));
    EXPECT_THAT(Prompt(*session, MakeInput("foo")),
                ElementsAre("UfooEM", IsPromptWithParams(3, 1.0)));
  }
  // Test creative
  {
    auto options = blink::mojom::AILanguageModelCreateOptions::New();
    options->sampling_mode =
        blink::mojom::AILanguageModelSamplingMode::kCreative;
    auto session = CreateSession(std::move(options));
    EXPECT_THAT(Prompt(*session, MakeInput("foo")),
                ElementsAre("UfooEM", IsPromptWithParams(10, 1.1)));
  }
  // Test most-creative
  {
    auto options = blink::mojom::AILanguageModelCreateOptions::New();
    options->sampling_mode =
        blink::mojom::AILanguageModelSamplingMode::kMostCreative;
    auto session = CreateSession(std::move(options));
    EXPECT_THAT(Prompt(*session, MakeInput("foo")),
                ElementsAre("UfooEM", IsPromptWithParams(25, 1.2)));
  }
}

TEST_F(AILanguageModelTest, SamplingModeDefault) {
  // Fallback to default values. Default values are omitted
  // from the output by the fake API in
  // services/on_device_model/fake/fake_chrome_ml_api.cc
  auto session = CreateSession();
  EXPECT_THAT(Prompt(*session, MakeInput("foo")), ElementsAre("UfooEM"));
}

TEST_F(AILanguageModelTest, SamplingParamsTopKOutOfRange) {
  auto sampling_params = blink::mojom::AILanguageModelSamplingParams::New();
  sampling_params->top_k = 0;
  sampling_params->temperature = 1.5f;

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->sampling_params = std::move(sampling_params);
  auto session = CreateSession(std::move(options));

  EXPECT_THAT(Prompt(*session, MakeInput("foo")),
              ElementsAre("UfooEM", "TopK: 1, Temp: 1.5"));
}

TEST_F(AILanguageModelTest, SamplingParamsTemperatureOutOfRange) {
  auto sampling_params = blink::mojom::AILanguageModelSamplingParams::New();
  sampling_params->top_k = 2;
  sampling_params->temperature = -1.0f;

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->sampling_params = std::move(sampling_params);
  auto session = CreateSession(std::move(options));

  EXPECT_THAT(Prompt(*session, MakeInput("foo")),
              ElementsAre("UfooEM", "TopK: 2, Temp: 0"));
}

TEST_F(AILanguageModelTest, MaxSamplingParams) {
  auto sampling_params = blink::mojom::AILanguageModelSamplingParams::New();
  sampling_params->top_k = kTestMaxTopK + 1;
  sampling_params->temperature = kTestMaxTemperature + 1;

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->sampling_params = std::move(sampling_params);
  auto session = CreateSession(std::move(options));

  EXPECT_THAT(Prompt(*session, MakeInput("foo")),
              ElementsAre("UfooEM", "TopK: 50, Temp: 1.5"));
}

TEST_F(AILanguageModelTest, InitialPrompts) {
  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->initial_prompts.push_back(MakePrompt(Role::kSystem, "hi"));
  options->initial_prompts.push_back(MakePrompt(Role::kUser, "bye"));
  auto session = CreateSession(std::move(options));

  EXPECT_THAT(Prompt(*session, MakeInput("foo")),
              ElementsAre("ShiEUbyeE", "UfooEM"));
}

TEST_F(AILanguageModelTest, InitialPromptsMultipleContents) {
  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->initial_prompts = MakeInput({"foo", "bar"});
  auto session = CreateSession(std::move(options));

  EXPECT_THAT(Prompt(*session, MakeInput("baz")),
              ElementsAre("UfoobarE", "UbazEM"));
}

TEST_F(AILanguageModelTest, InitialPromptsInstanceInfo) {
  TestCreateLanguageModelClient language_model_client;
  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->initial_prompts.push_back(MakePrompt(Role::kSystem, "hi"));
  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(), std::move(options));

  auto result = language_model_client.result().Take();
  ASSERT_OK(result);

  auto info = std::move(result.value().info);
  EXPECT_EQ(info->input_quota, kTestMaxTokens);
  EXPECT_EQ(info->input_usage, std::strlen("ShiE"));
}

TEST_F(AILanguageModelTest, InitialPromptsTooLarge) {
  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->initial_prompts.push_back(
      MakePrompt(Role::kSystem, std::string(kTestMaxTokens + 1, 'a')));

  TestCreateLanguageModelClient language_model_client;
  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(), std::move(options));

  auto result = language_model_client.result().Take();
  EXPECT_FALSE(result.has_value());

  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kInitialInputTooLarge);
  ASSERT_GT(result.error().quota_error_info->requested, kTestMaxTokens);
  ASSERT_EQ(result.error().quota_error_info->quota, kTestMaxTokens);
}

TEST_F(AILanguageModelTest, CreateResolvesAfterInitialPromptsAreAppended) {
  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->initial_prompts.push_back(MakePrompt(Role::kSystem, "hi"));

  fake_broker_->settings().set_append_delay(base::Seconds(5));

  TestCreateLanguageModelClient language_model_client;
  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(), std::move(options));

  // Creation will not be complete yet, because Append is delayed.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(language_model_client.result().IsReady());

  // Fast forward time to allow Append to complete.
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_TRUE(language_model_client.result().IsReady());
}

TEST_F(AILanguageModelTest, InputTooLarge) {
  auto session = CreateSession();

  AITestUtils::TestStreamingResponder responder;
  session->Prompt(MakeInput(std::string(kTestMaxTokens + 1, 'a')), nullptr,
                  responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorInputTooLarge);
  ASSERT_GT(responder.quota_error_info().requested, kTestMaxTokens);
  ASSERT_EQ(responder.quota_error_info().quota, kTestMaxTokens);
}

TEST_F(AILanguageModelTest, QuotaOverflowOnPromptInput) {
  // Set the execute result so the long prompt is not echoed back as the
  // response.
  fake_broker_->settings().set_execute_result({"hi"});
  // Initial prompt should be kept on overflow.
  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->initial_prompts.push_back(MakePrompt(Role::kSystem, "init"));
  auto session = CreateSession(std::move(options));
  // Set a prompt that is close to max token length. This string should be
  // stripped from the prompt history, while the initial prompts and
  // `long_prompt` will be kept.
  EXPECT_THAT(
      Prompt(*session, MakeInput(std::string(kTestMaxTokens - 20, 'a'))),
      ElementsAre("hi"));
  EXPECT_THAT(Prompt(*session, MakeInput("foo")), ElementsAre("hi"));

  // Clear execute result so we can verify the input by checking the response.
  fake_broker_->settings().set_execute_result({});
  std::string long_prompt(kTestMaxTokens / 3, 'a');
  AITestUtils::TestStreamingResponder responder;
  session->Prompt(MakeInput(long_prompt), nullptr, responder.BindRemote());
  responder.WaitForContextOverflow();
  EXPECT_TRUE(responder.WaitForCompletion());
  // Response should include input/output of previous prompt with the original
  // long prompt not present.
  EXPECT_THAT(responder.responses(),
              ElementsAre("SinitE", "UfooEMhiE", "U" + long_prompt + "EM"));
}

TEST_F(AILanguageModelTest, QuotaOverflowOnAppend) {
  // Initial prompt should be kept on overflow.
  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->initial_prompts.push_back(MakePrompt(Role::kSystem, "init"));
  auto session = CreateSession(std::move(options));
  // Set a prompt that is close to max token length.
  Append(*session, MakeInput(std::string(kTestMaxTokens - 20, 'a')));

  std::string long_prompt(kTestMaxTokens / 3, 'a');
  AITestUtils::TestStreamingResponder responder;
  session->Append(MakeInput(long_prompt), responder.BindRemote());
  responder.WaitForContextOverflow();
  EXPECT_TRUE(responder.WaitForCompletion());

  EXPECT_THAT(Prompt(*session, MakeInput("foo")),
              ElementsAre("SinitE", "U" + long_prompt + "E", "UfooEM"));
}

TEST_F(AILanguageModelTest, QuotaOverflowOnOutput) {
  // Set the execute result so the long prompt is not echoed back as the
  // response.
  fake_broker_->settings().set_execute_result({"hi"});
  auto session = CreateSession();
  // Set a prompt that is close to max token length. This string should be
  // stripped from the prompt history, while the next prompt's input and output
  // will be kept.
  EXPECT_THAT(
      Prompt(*session, MakeInput(std::string(kTestMaxTokens - 20, 'a'))),
      ElementsAre("hi"));

  // Reset result to a long response that should cause overflow. `long_response`
  // should be kept, but the previous prompt will be removed.
  std::string long_response(kTestMaxTokens / 3, 'a');
  fake_broker_->settings().set_execute_result({long_response});

  AITestUtils::TestStreamingResponder responder;
  session->Prompt(MakeInput("foo"), nullptr, responder.BindRemote());
  responder.WaitForContextOverflow();
  EXPECT_TRUE(responder.WaitForCompletion());
  EXPECT_THAT(responder.responses(), ElementsAre(long_response));

  // Verify the original long response was removed. The response should contain:
  // - "foo"+long_response from the previous prompt call
  // - "bar" from the current prompt call
  fake_broker_->settings().set_execute_result({});
  EXPECT_THAT(Prompt(*session, MakeInput("bar")),
              ElementsAre("UfooEM" + long_response + "E", "UbarEM"));
}

TEST_F(AILanguageModelTest, OutputOverflowsModelMaxTokens) {
  auto session = CreateSession();
  // Add a prompt to start, this should be kept after the overflow.
  EXPECT_THAT(Prompt(*session, MakeInput("foo")), ElementsAreArray({"UfooEM"}));

  // Set a fake response that will overrun the max model tokens.
  fake_broker_->settings().set_execute_result(
      {std::string(2 * optimization_guide::kOnDeviceModelMaxTokens, 'a')});
  AITestUtils::TestStreamingResponder responder;
  session->Prompt(MakeInput("bar"), nullptr, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::
                kErrorResponseExceedsMaxTokens);

  // Now prompt again, the failed prompt should not be present.
  fake_broker_->settings().set_execute_result({});
  EXPECT_THAT(Prompt(*session, MakeInput("baz")),
              ElementsAreArray({"UfooEM", "UbazEM"}));
}

TEST_F(AILanguageModelTest, OutputOverflowsAdditionalBuffer) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Use a smaller output buffer to test the value is used correctly.
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{features::kAILanguageModelOverrideConfiguration,
        {{"ai_language_model_output_buffer", "10"}}}},
      {});
  auto session = CreateSession();
  // Append an input that is just below max tokens, the next output should
  // overflow the buffer and cause an error.
  Append(*session, MakeInput(std::string(kTestMaxTokens - 5, 'a')));

  // Create a response that will be just larger than the output buffer.
  fake_broker_->settings().set_execute_result({std::string(15, 'a')});
  AITestUtils::TestStreamingResponder responder;
  session->Prompt(MakeInput(""), nullptr, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::
                kErrorResponseExceedsMaxTokens);
}

TEST_F(AILanguageModelTest, OutputOverflowsContextMaxTokens) {
  auto session = CreateSession();
  // Add a prompt to start, this should be kept after the overflow.
  EXPECT_THAT(Prompt(*session, MakeInput("foo")), ElementsAreArray({"UfooEM"}));

  // Set a fake response that will overflow the maximum context size.
  fake_broker_->settings().set_execute_result(
      {std::string(kTestMaxTokens, 'a')});
  AITestUtils::TestStreamingResponder responder;
  session->Prompt(MakeInput("bar"), nullptr, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::
                kErrorResponseExceedsRemainingContext);

  // Now prompt again, the failed prompt should not be present.
  fake_broker_->settings().set_execute_result({});
  EXPECT_THAT(Prompt(*session, MakeInput("baz")),
              ElementsAreArray({"UfooEM", "UbazEM"}));
}

TEST_F(AILanguageModelTest, DisconnectErrorUnknown) {
  auto session = CreateSession();
  fake_broker_->settings().set_execute_error(
      on_device_model::mojom::GenerateError::kUnknown);
  AITestUtils::TestStreamingResponder responder;
  session->Prompt(MakeInput("foo"), nullptr, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorUnknown);
}

TEST_F(AILanguageModelTest, DisconnectErrorInvalidConstraint) {
  auto session = CreateSession();
  fake_broker_->settings().set_execute_error(
      on_device_model::mojom::GenerateError::kInvalidConstraint);
  AITestUtils::TestStreamingResponder responder;
  session->Prompt(MakeInput("foo"), nullptr, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorInvalidRequest);
}

TEST_F(AILanguageModelTest, Destroy) {
  auto session = CreateSession();
  base::RunLoop run_loop;
  session.set_disconnect_handler(run_loop.QuitClosure());
  EXPECT_THAT(Prompt(*session, MakeInput("foo")), ElementsAreArray({"UfooEM"}));
  session->Destroy();
  run_loop.Run();
}

TEST_F(AILanguageModelTest, DestroyWithActivePrompt) {
  fake_broker_->settings().set_execute_delay(base::Minutes(1));
  auto session = CreateSession();
  base::RunLoop run_loop;
  session.set_disconnect_handler(run_loop.QuitClosure());

  AITestUtils::TestStreamingResponder responder;
  session->Prompt(MakeInput("foo"), nullptr, responder.BindRemote());
  session->Destroy();
  run_loop.Run();

  EXPECT_FALSE(responder.WaitForCompletion());
}

struct LanguageParams {
  std::string enabled_languages;
  std::vector<std::string> expected_input_language;
  bool expect_error;
};

std::ostream& operator<<(std::ostream& os, const LanguageParams& params) {
  // Print the desired data members of params to the output stream (os)
  os << "enabled_languages:" << params.enabled_languages
     << ", expected_input_language:"
     << base::JoinString(params.expected_input_language, ", ")
     << ", expect_error:" << params.expect_error;
  return os;  // Return the ostream reference for chaining
}

class AILanguageModelTestWithLanguageParams
    : public AILanguageModelTest,
      public testing::WithParamInterface<LanguageParams> {
 public:
  AILanguageModelTestWithLanguageParams() {
    features_.InitWithFeaturesAndParameters(
        {{blink::features::kAIPromptAPI,
          {{"langs", GetParam().enabled_languages}}}},
        {});
  }
  base::test::ScopedFeatureList features_;
};

TEST_P(AILanguageModelTestWithLanguageParams, PromptWithEnabledLanguages) {
  auto expected_input = blink::mojom::AILanguageModelExpected::New();
  expected_input->languages.emplace();
  for (const auto& language : GetParam().expected_input_language) {
    expected_input->languages->push_back(
        blink::mojom::AILanguageCode::New(language));
  }

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->expected_inputs.emplace();
  options->expected_inputs->push_back(std::move(expected_input));

  TestCreateLanguageModelClient language_model_client;
  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(), std::move(options));

  auto result = language_model_client.result().Take();
  if (GetParam().expect_error) {
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().error,
              blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage);
  } else {
    EXPECT_OK(result);
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    AILanguageModelTestWithLanguageParams,
    ::testing::Values(LanguageParams{"en,es,ja", {"en"}, false},
                      LanguageParams{"*", {"en"}, false},
                      LanguageParams{"*", {"fr"}, false},
                      LanguageParams{"", {"en"}, false},
                      LanguageParams{"", {"de"}, false},
                      LanguageParams{"", {"tlh"}, true},
                      LanguageParams{"es,ja", {"es"}, false},
                      LanguageParams{"en,es,ja", {"ja"}, false},
                      LanguageParams{"en,es,ja", {"ja", "es"}, false},
                      LanguageParams{"en,es,ja", {"ja", "tlh"}, true},
                      LanguageParams{"en,es,ja,de", {"de"}, false}));

TEST_F(AILanguageModelTest, UnsupportedInputCapability) {
  TestCreateLanguageModelClient language_model_client;
  auto expected_input = blink::mojom::AILanguageModelExpected::New();
  expected_input->type = blink::mojom::AILanguageModelPromptType::kImage;

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->expected_inputs.emplace();
  options->expected_inputs->push_back(std::move(expected_input));
  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(), std::move(options));

  auto result = language_model_client.result().Take();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

TEST_F(AILanguageModelTest, UnsupportedOutputCapability) {
  auto expected_output = blink::mojom::AILanguageModelExpected::New();
  expected_output->type = blink::mojom::AILanguageModelPromptType::kImage;

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->expected_outputs.emplace();
  options->expected_outputs->push_back(std::move(expected_output));
  TestCreateLanguageModelClient language_model_client;
  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(), std::move(options));

  auto result = language_model_client.result().Take();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

TEST_F(AILanguageModelTest, MultimodalInputImageNotSpecified) {
  fake_broker_->InstallBaseModel(
      {.config = optimization_guide::ExecutionConfigWithCapabilities(
           {optimization_guide::proto::OnDeviceModelCapability::
                ON_DEVICE_MODEL_CAPABILITY_IMAGE_INPUT,
            optimization_guide::proto::OnDeviceModelCapability::
                ON_DEVICE_MODEL_CAPABILITY_AUDIO_INPUT})});

  auto audio_input = blink::mojom::AILanguageModelExpected::New();
  audio_input->type = blink::mojom::AILanguageModelPromptType::kAudio;
  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->expected_inputs.emplace();
  options->expected_inputs->push_back(std::move(audio_input));
  auto session = CreateSession(std::move(options));

  auto make_input = [] {
    std::vector<blink::mojom::AILanguageModelPromptPtr> input =
        MakeInput("foo");
    input.push_back(blink::mojom::AILanguageModelPrompt::New(
        Role::kUser,
        ToVector(blink::mojom::AILanguageModelPromptContent::NewBitmap(
            CreateTestBitmap(10, 10))),
        /*is_prefix=*/false));
    return input;
  };
  {
    AITestUtils::TestStreamingResponder responder;
    session->Prompt(make_input(), nullptr, responder.BindRemote());
    EXPECT_FALSE(responder.WaitForCompletion());
    EXPECT_EQ(responder.error_status(),
              blink::mojom::ModelStreamingResponseStatus::kErrorInvalidRequest);
  }
  {
    AITestUtils::TestStreamingResponder responder;
    session->Append(make_input(), responder.BindRemote());
    EXPECT_FALSE(responder.WaitForCompletion());
    EXPECT_EQ(responder.error_status(),
              blink::mojom::ModelStreamingResponseStatus::kErrorInvalidRequest);
  }
  base::test::TestFuture<std::optional<uint32_t>> measure_future;
  session->MeasureInputUsage(make_input(), measure_future.GetCallback());
  EXPECT_EQ(measure_future.Get(), std::nullopt);
}

TEST_F(AILanguageModelTest, MultimodalInputAudioNotSpecified) {
  fake_broker_->InstallBaseModel(
      {.config = optimization_guide::ExecutionConfigWithCapabilities(
           {optimization_guide::proto::OnDeviceModelCapability::
                ON_DEVICE_MODEL_CAPABILITY_IMAGE_INPUT,
            optimization_guide::proto::OnDeviceModelCapability::
                ON_DEVICE_MODEL_CAPABILITY_AUDIO_INPUT})});

  auto image_input = blink::mojom::AILanguageModelExpected::New();
  image_input->type = blink::mojom::AILanguageModelPromptType::kImage;
  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->expected_inputs.emplace();
  options->expected_inputs->push_back(std::move(image_input));
  auto session = CreateSession(std::move(options));

  auto make_input = [] {
    std::vector<blink::mojom::AILanguageModelPromptPtr> input =
        MakeInput("foo");
    input.push_back(blink::mojom::AILanguageModelPrompt::New(
        Role::kUser,
        ToVector(blink::mojom::AILanguageModelPromptContent::NewAudio(
            CreateTestAudio())),
        /*is_prefix=*/false));
    return input;
  };
  {
    AITestUtils::TestStreamingResponder responder;
    session->Prompt(make_input(), nullptr, responder.BindRemote());
    EXPECT_FALSE(responder.WaitForCompletion());
    EXPECT_EQ(responder.error_status(),
              blink::mojom::ModelStreamingResponseStatus::kErrorInvalidRequest);
  }
  {
    AITestUtils::TestStreamingResponder responder;
    session->Append(make_input(), responder.BindRemote());
    EXPECT_FALSE(responder.WaitForCompletion());
    EXPECT_EQ(responder.error_status(),
              blink::mojom::ModelStreamingResponseStatus::kErrorInvalidRequest);
  }
  base::test::TestFuture<std::optional<uint32_t>> measure_future;
  session->MeasureInputUsage(make_input(), measure_future.GetCallback());
  EXPECT_EQ(measure_future.Get(), std::nullopt);
}

TEST_F(AILanguageModelTest, MultimodalInput) {
  fake_broker_->InstallBaseModel(
      {.config = optimization_guide::ExecutionConfigWithCapabilities(
           {optimization_guide::proto::OnDeviceModelCapability::
                ON_DEVICE_MODEL_CAPABILITY_IMAGE_INPUT,
            optimization_guide::proto::OnDeviceModelCapability::
                ON_DEVICE_MODEL_CAPABILITY_AUDIO_INPUT})});

  auto audio_input = blink::mojom::AILanguageModelExpected::New();
  audio_input->type = blink::mojom::AILanguageModelPromptType::kAudio;
  auto image_input = blink::mojom::AILanguageModelExpected::New();
  image_input->type = blink::mojom::AILanguageModelPromptType::kImage;

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->expected_inputs.emplace();
  options->expected_inputs->push_back(std::move(audio_input));
  options->expected_inputs->push_back(std::move(image_input));
  auto session = CreateSession(std::move(options));

  std::vector<blink::mojom::AILanguageModelPromptPtr> input = MakeInput("foo");
  input.push_back(blink::mojom::AILanguageModelPrompt::New(
      Role::kUser,
      ToVector(blink::mojom::AILanguageModelPromptContent::NewBitmap(
          CreateTestBitmap(10, 10))),
      /*is_prefix=*/false));
  input.push_back(blink::mojom::AILanguageModelPrompt::New(
      Role::kUser,
      ToVector(blink::mojom::AILanguageModelPromptContent::NewAudio(
          CreateTestAudio())),
      /*is_prefix=*/false));
  EXPECT_THAT(Prompt(*session, std::move(input)),
              ElementsAreArray({"UfooEU<image>EU<audio>EM"}));
}

TEST_F(AILanguageModelTest, ModelDownload) {
  MockDownloadProgressObserver observer;
  GetAIManagerInterface()->AddModelDownloadProgressObserver(
      observer.BindNewPipeAndPassRemote());
  fake_broker_->component_state().WaitForDownloadObserver();

  // Receives the zero update.
  uint64_t total_bytes =
      fake_broker_->component_state().component().total_bytes();
  fake_broker_->component_state().UpdateDownloadProgress(0);
  observer.ExpectReceivedNormalizedUpdate(0, total_bytes);

  // Receives an update for normalized to the total bytes.
  task_environment()->FastForwardBy(base::Milliseconds(51));
  uint64_t downloaded_bytes = total_bytes / 2;
  fake_broker_->component_state().UpdateDownloadProgress(downloaded_bytes);
  observer.ExpectReceivedNormalizedUpdate(downloaded_bytes, total_bytes);

  // Receives the final one update.
  task_environment()->FastForwardBy(base::Milliseconds(51));
  fake_broker_->component_state().UpdateDownloadProgress(total_bytes);
  observer.ExpectReceivedNormalizedUpdate(total_bytes, total_bytes);
}

TEST_F(AILanguageModelTest, MeasureInputUsage) {
  auto session = CreateSession();
  base::test::TestFuture<std::optional<uint32_t>> measure_future;
  session->MeasureInputUsage(MakeInput("foo"), measure_future.GetCallback());
  EXPECT_EQ(measure_future.Get(), std::string("UfooE").size());
}

TEST_F(AILanguageModelTest, TextSafetyInitialPrompts) {
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);
  optimization_guide::FakeSafetyModelAsset safety_asset(CreateSafetyConfig());
  fake_broker_->UpdateSafetyModel(safety_asset);

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->initial_prompts.push_back(MakePrompt(Role::kSystem, "unsafe"));

  TestCreateLanguageModelClient language_model_client;
  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(), std::move(options));
  auto result = language_model_client.result().Take();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

TEST_F(AILanguageModelTest, TextSafetyInput) {
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);
  optimization_guide::FakeSafetyModelAsset safety_asset(CreateSafetyConfig());
  fake_broker_->UpdateSafetyModel(safety_asset);

  fake_broker_->settings().set_execute_result({"hi"});
  auto session = CreateSession();
  EXPECT_THAT(Prompt(*session, MakeInput("safe")), ElementsAre("hi"));

  // Fake text safety checker looks for the string "unsafe".
  AITestUtils::TestStreamingResponder responder;
  session->Prompt(MakeInput("unsafe"), nullptr, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
}

TEST_F(AILanguageModelTest, TextSafetyOutput) {
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);
  optimization_guide::FakeSafetyModelAsset safety_asset([] {
    auto safety_config = CreateSafetyConfig();
    safety_config.mutable_partial_output_checks()->set_minimum_tokens(1000);
    return safety_config;
  }());
  fake_broker_->UpdateSafetyModel(safety_asset);

  // Fake text safety checker looks for the string "unsafe".
  fake_broker_->settings().set_execute_result(
      {"a", "b", "c", "d", "e", "f", "g", "unsafe", "h"});
  auto session = CreateSession();
  AITestUtils::TestStreamingResponder responder;
  session->Prompt(MakeInput("foo"), nullptr, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
  EXPECT_TRUE(responder.responses().empty());
}

TEST_F(AILanguageModelTest, TextSafetyOutputPartial) {
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);
  optimization_guide::FakeSafetyModelAsset safety_asset([] {
    auto safety_config = CreateSafetyConfig();
    safety_config.mutable_partial_output_checks()->set_minimum_tokens(3);
    safety_config.mutable_partial_output_checks()->set_token_interval(2);
    return safety_config;
  }());
  fake_broker_->UpdateSafetyModel(safety_asset);

  // Fake text safety checker looks for the string "unsafe".
  fake_broker_->settings().set_execute_result(
      {"a", "b", "c", "d", "e", "f", "g", "unsafe", "h"});
  auto session = CreateSession();
  AITestUtils::TestStreamingResponder responder;
  session->Prompt(MakeInput("foo"), nullptr, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
  // Partial checks should still allow some output to stream.
  EXPECT_THAT(responder.responses(), ElementsAre("abc", "de", "fg"));
}

TEST_F(AILanguageModelTest, QueuesOperations) {
  auto session = CreateSession();
  AITestUtils::TestStreamingResponder responder1;
  AITestUtils::TestStreamingResponder responder2;
  AITestUtils::TestStreamingResponder responder3;
  // Add three prompts and a fork, all these operations should complete
  // successfully and in order.
  session->Prompt(MakeInput("foo"), nullptr, responder1.BindRemote());
  session->Prompt(MakeInput("bar"), nullptr, responder2.BindRemote());

  TestCreateLanguageModelClient fork_client;
  session->Fork(fork_client.BindNewPipeAndPassRemote());
  session->Prompt(MakeInput("baz"), nullptr, responder3.BindRemote());

  EXPECT_TRUE(responder1.WaitForCompletion());
  EXPECT_THAT(responder1.responses(), ElementsAreArray({"UfooEM"}));

  EXPECT_TRUE(responder2.WaitForCompletion());
  EXPECT_THAT(responder2.responses(), ElementsAreArray({"UfooEM", "UbarEM"}));

  EXPECT_TRUE(responder3.WaitForCompletion());
  EXPECT_THAT(responder3.responses(),
              ElementsAreArray({"UfooEM", "UbarEM", "UbazEM"}));

  auto fork_future = fork_client.result().Take();
  ASSERT_OK(fork_future);
  mojo::Remote<blink::mojom::AILanguageModel> fork_model =
      mojo::Remote<blink::mojom::AILanguageModel>(
          std::move(fork_future.value().language_model));
  EXPECT_THAT(Prompt(*fork_model, MakeInput("fork")),
              ElementsAreArray({"UfooEM", "UbarEM", "UforkEM"}));
}

TEST_F(AILanguageModelTest, Constraint) {
  auto session = CreateSession();
  EXPECT_THAT(
      Prompt(*session, MakeInput("foo"),
             on_device_model::mojom::ResponseConstraint::NewRegex("reg")),
      ElementsAre("Hint: constrained_decoding ", "Constraint: regex reg",
                  "UfooEM"));
}

TEST_F(AILanguageModelTest, Prefix) {
  auto session = CreateSession();
  std::vector<blink::mojom::AILanguageModelPromptPtr> prompts;
  prompts.push_back(MakePrompt(Role::kUser, "foo"));
  prompts.push_back(MakePrompt(Role::kAssistant, "bar", /*is_prefix=*/true));
  // Expect no 'bar' end token, nor separate model response start token.
  EXPECT_THAT(Prompt(*session, std::move(prompts)), ElementsAre("UfooEMbar"));
}

TEST_F(AILanguageModelTest, ServiceCrash) {
  auto session = CreateSession();
  AITestUtils::TestStreamingResponder responder;
  session->Prompt(MakeInput("bar"), nullptr, responder.BindRemote());
  fake_broker_->CrashService();
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);

  // Recreating the session should be fine.
  session = CreateSession();
  EXPECT_THAT(Prompt(*session, MakeInput("foo")), ElementsAreArray({"UfooEM"}));
}

TEST_F(AILanguageModelTest, CrashRecovery) {
  auto session = CreateSession();
  Append(*session, MakeInput("foo"));

  fake_broker_->CrashService();

  EXPECT_THAT(Prompt(*session, MakeInput("bar")),
              ElementsAre("UfooE", "UbarEM"));
}

TEST_F(AILanguageModelTest, CrashRecoveryWithMultipleCrashes) {
  auto session = CreateSession();
  Append(*session, MakeInput("foo"));
  fake_broker_->CrashService();

  Append(*session, MakeInput("bar"));
  fake_broker_->CrashService();

  EXPECT_THAT(Prompt(*session, MakeInput("baz")),
              ElementsAre("UfooEUbarE", "UbazEM"));
}

TEST_F(AILanguageModelTest, CrashRecoveryWithInitialPrompts) {
  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->initial_prompts.push_back(MakePrompt(Role::kSystem, "hi"));
  auto session = CreateSession(std::move(options));
  Append(*session, MakeInput("foo"));

  fake_broker_->CrashService();

  EXPECT_THAT(Prompt(*session, MakeInput("bar")),
              ElementsAre("ShiE", "UfooE", "UbarEM"));
}

TEST_F(AILanguageModelTest, CrashRecoveryMeasureInputUsage) {
  auto session = CreateSession();
  Append(*session, MakeInput("foo"));

  fake_broker_->CrashService();

  base::test::TestFuture<std::optional<uint32_t>> measure_future;
  session->MeasureInputUsage(MakeInput("foo"), measure_future.GetCallback());
  EXPECT_EQ(measure_future.Get(), std::string("UfooE").size());
}

TEST_F(AILanguageModelTest, CanCreate_DefaultOptions) {
  blink::mojom::AILanguageModelCreateOptionsPtr options =
      blink::mojom::AILanguageModelCreateOptions::New();

  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    GetAIManagerInterface()->CanCreateLanguageModel(options.Clone(),
                                                    future.GetCallback());
    EXPECT_EQ(future.Get(),
              blink::mojom::ModelAvailabilityCheckResult::kDownloadable);
  }

  // After model is ready, `CanCreateLanguageModel` should return available.
  EnsureModelIsReady();

  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    GetAIManagerInterface()->CanCreateLanguageModel(options.Clone(),
                                                    future.GetCallback());
    EXPECT_EQ(future.Get(),
              blink::mojom::ModelAvailabilityCheckResult::kAvailable);
  }
}

TEST_F(AILanguageModelTest, CanCreate_SupportedLanguages) {
  EnsureModelIsReady();

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->expected_inputs.emplace();
  options->expected_inputs->push_back(
      blink::mojom::AILanguageModelExpected::New(
          blink::mojom::AILanguageModelPromptType::kText,
          AITestUtils::ToMojoLanguageCodes({"en"})));
  options->expected_outputs.emplace();
  options->expected_outputs->push_back(
      blink::mojom::AILanguageModelExpected::New(
          blink::mojom::AILanguageModelPromptType::kText,
          AITestUtils::ToMojoLanguageCodes({"en"})));

  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  GetAIManagerInterface()->CanCreateLanguageModel(std::move(options),
                                                  future.GetCallback());
  EXPECT_EQ(future.Get(),
            blink::mojom::ModelAvailabilityCheckResult::kAvailable);
}

TEST_F(AILanguageModelTest, CanCreate_UnsupportedInputLanguages) {
  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->expected_inputs.emplace();
  options->expected_inputs->push_back(
      blink::mojom::AILanguageModelExpected::New(
          blink::mojom::AILanguageModelPromptType::kText,
          AITestUtils::ToMojoLanguageCodes({"tlh"})));
  GetAIManagerInterface()->CanCreateLanguageModel(std::move(options),
                                                  future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                              kUnavailableUnsupportedLanguage);
}

TEST_F(AILanguageModelTest, CanCreate_UnsupportedOutputLanguages) {
  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->expected_outputs.emplace();
  options->expected_outputs->push_back(
      blink::mojom::AILanguageModelExpected::New(
          blink::mojom::AILanguageModelPromptType::kText,
          AITestUtils::ToMojoLanguageCodes({"tlh"})));
  GetAIManagerInterface()->CanCreateLanguageModel(std::move(options),
                                                  future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                              kUnavailableUnsupportedLanguage);
}

TEST_F(AILanguageModelTest, CanCreate_TextInputCapabilities) {
  blink::mojom::AILanguageModelCreateOptionsPtr options =
      blink::mojom::AILanguageModelCreateOptions::New();

  EnsureModelIsReady();

  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    GetAIManagerInterface()->CanCreateLanguageModel(options.Clone(),
                                                    future.GetCallback());
    EXPECT_EQ(future.Get(),
              blink::mojom::ModelAvailabilityCheckResult::kAvailable);
  }
  {
    auto image_input = blink::mojom::AILanguageModelExpected::New();
    image_input->type = blink::mojom::AILanguageModelPromptType::kImage;
    options->expected_inputs.emplace();
    options->expected_inputs->push_back(std::move(image_input));

    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    GetAIManagerInterface()->CanCreateLanguageModel(options.Clone(),
                                                    future.GetCallback());
    EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableModelAdaptationNotAvailable);
  }

  {
    auto audio_input = blink::mojom::AILanguageModelExpected::New();
    audio_input->type = blink::mojom::AILanguageModelPromptType::kAudio;
    options->expected_inputs.emplace();
    options->expected_inputs->push_back(std::move(audio_input));

    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    GetAIManagerInterface()->CanCreateLanguageModel(options.Clone(),
                                                    future.GetCallback());
    EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableModelAdaptationNotAvailable);
  }
}

TEST_F(AILanguageModelTest, CanCreate_ImageAndAudioInputCapabilities) {
  fake_broker_->InstallBaseModel(
      {.config = optimization_guide::ExecutionConfigWithCapabilities(
           {optimization_guide::proto::OnDeviceModelCapability::
                ON_DEVICE_MODEL_CAPABILITY_IMAGE_INPUT,
            optimization_guide::proto::OnDeviceModelCapability::
                ON_DEVICE_MODEL_CAPABILITY_AUDIO_INPUT})});

  EnsureModelIsReady();

  blink::mojom::AILanguageModelCreateOptionsPtr options =
      blink::mojom::AILanguageModelCreateOptions::New();
  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    GetAIManagerInterface()->CanCreateLanguageModel(options.Clone(),
                                                    future.GetCallback());
    EXPECT_EQ(future.Get(),
              blink::mojom::ModelAvailabilityCheckResult::kAvailable);
  }
  {
    auto image_input = blink::mojom::AILanguageModelExpected::New();
    image_input->type = blink::mojom::AILanguageModelPromptType::kImage;
    options->expected_inputs.emplace();
    options->expected_inputs->push_back(std::move(image_input));

    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    GetAIManagerInterface()->CanCreateLanguageModel(options.Clone(),
                                                    future.GetCallback());
    EXPECT_EQ(future.Get(),
              blink::mojom::ModelAvailabilityCheckResult::kAvailable);
  }
  {
    auto audio_input = blink::mojom::AILanguageModelExpected::New();
    audio_input->type = blink::mojom::AILanguageModelPromptType::kAudio;
    options->expected_inputs->push_back(std::move(audio_input));

    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    GetAIManagerInterface()->CanCreateLanguageModel(options.Clone(),
                                                    future.GetCallback());
    EXPECT_EQ(future.Get(),
              blink::mojom::ModelAvailabilityCheckResult::kAvailable);
  }
}

TEST_F(AILanguageModelTest, CanCreate_DeviceCapabilities) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{optimization_guide::features::kOnDeviceModelPerformanceParams,
        {{"compatible_on_device_performance_classes", "3,4,5,6"}}}},
      {{on_device_model::features::kOnDeviceModelCpuBackend}});

  fake_broker_->service_settings().performance_class =
      PerformanceClass::kVeryLow;

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  {
    auto image_input = blink::mojom::AILanguageModelExpected::New();
    image_input->type = blink::mojom::AILanguageModelPromptType::kImage;
    options->expected_inputs.emplace();
    options->expected_inputs->push_back(std::move(image_input));

    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    GetAIManagerInterface()->CanCreateLanguageModel(options.Clone(),
                                                    future.GetCallback());
    EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableModelAdaptationNotAvailable);
  }

  {
    auto audio_input = blink::mojom::AILanguageModelExpected::New();
    audio_input->type = blink::mojom::AILanguageModelPromptType::kAudio;
    options->expected_inputs.emplace();
    options->expected_inputs->push_back(std::move(audio_input));

    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    GetAIManagerInterface()->CanCreateLanguageModel(options.Clone(),
                                                    future.GetCallback());
    EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableModelAdaptationNotAvailable);
  }
}

TEST_F(AILanguageModelTest, CanCreate_DeviceAudioCapabilities) {
  fake_broker_->service_settings().vram_mb =
      optimization_guide::kOnDeviceModelAudioVramMinMb - 1;

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  {
    auto image_input = blink::mojom::AILanguageModelExpected::New();
    image_input->type = blink::mojom::AILanguageModelPromptType::kImage;
    options->expected_inputs.emplace();
    options->expected_inputs->push_back(std::move(image_input));

    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    GetAIManagerInterface()->CanCreateLanguageModel(options.Clone(),
                                                    future.GetCallback());
    EXPECT_EQ(future.Get(),
              blink::mojom::ModelAvailabilityCheckResult::kDownloadable);
  }

  {
    auto audio_input = blink::mojom::AILanguageModelExpected::New();
    audio_input->type = blink::mojom::AILanguageModelPromptType::kAudio;
    options->expected_inputs.emplace();
    options->expected_inputs->push_back(std::move(audio_input));

    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    GetAIManagerInterface()->CanCreateLanguageModel(options.Clone(),
                                                    future.GetCallback());
    EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableModelAdaptationNotAvailable);
  }
}

TEST_F(AILanguageModelTest, CreateLanguageModelModelNotEligible) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{optimization_guide::features::kOnDeviceModelPerformanceParams,
        {{"compatible_on_device_performance_classes", "3,4,5,6"}}}},
      {{on_device_model::features::kOnDeviceModelCpuBackend}});

  fake_broker_->service_settings().performance_class =
      PerformanceClass::kVeryLow;

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  TestCreateLanguageModelClient language_model_client;
  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(), std::move(options));

  auto result = language_model_client.result().Take();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

TEST_F(AILanguageModelTest, CreateLanguageModelWaitsForBaseModel) {
  fake_broker_->InstallBaseModel(nullptr);

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  TestCreateLanguageModelClient language_model_client;
  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(), std::move(options));

  auto& future = language_model_client.result();
  task_environment()->FastForwardBy(base::Hours(1));
  EXPECT_FALSE(future.IsReady());

  fake_broker_->InstallBaseModel(
      std::make_unique<optimization_guide::FakeBaseModelAsset>());

  EXPECT_OK(future.Take());
}

TEST_F(AILanguageModelTest, CreateLanguageModelWaitsForModelAdaptation) {
  fake_broker_->model_provider().RemoveModel(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_PROMPT_API);

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  TestCreateLanguageModelClient language_model_client;
  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(), std::move(options));

  auto& future = language_model_client.result();
  task_environment()->FastForwardBy(base::Hours(1));
  EXPECT_FALSE(future.IsReady());

  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);

  EXPECT_OK(future.Take());
}

TEST_F(AILanguageModelTest, CreateLanguageModelWaitsForTextSafetyModel) {
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  TestCreateLanguageModelClient language_model_client;
  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(), std::move(options));

  auto& future = language_model_client.result();
  task_environment()->FastForwardBy(base::Hours(1));
  EXPECT_FALSE(future.IsReady());

  optimization_guide::FakeSafetyModelAsset safety_asset(CreateSafetyConfig());
  fake_broker_->UpdateSafetyModel(safety_asset);

  EXPECT_OK(future.Take());
}

TEST_F(AILanguageModelTest, CreateLanguageModelSafetyConfigNotAvailable) {
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);
  // Provide a safety asset that does not support prompt.
  optimization_guide::FakeSafetyModelAsset safety_asset([] {
    auto safety_config = CreateSafetyConfig();
    safety_config.set_feature(
        optimization_guide::proto::MODEL_EXECUTION_FEATURE_TEST);
    return safety_config;
  }());
  fake_broker_->UpdateSafetyModel(safety_asset);

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  TestCreateLanguageModelClient language_model_client;
  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(), std::move(options));

  auto result = language_model_client.result().Take();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

// Tests the `AILanguageModel::Context` that's initialized with/without any
// initial prompt.
class AILanguageModelContextTest : public testing::Test {
 public:
  AILanguageModel::Context context_{kTestMaxContextToken};
};

// Tests `GetContextString()` when the context is empty.
TEST_F(AILanguageModelContextTest, TestContextOperation_Empty) {
  EXPECT_EQ(GetContextString(context_), "");
}

// Tests `GetContextString()` when some items are added to the context.
TEST_F(AILanguageModelContextTest, TestContextOperation_NonEmpty) {
  EXPECT_EQ(context_.AddContextItem(SimpleContextItem("test", 1u)),
            AILanguageModel::Context::SpaceReservationResult::kSufficientSpace);
  EXPECT_EQ(GetContextString(context_), "S: test");

  context_.AddContextItem(SimpleContextItem(" test again", 2u));
  EXPECT_EQ(GetContextString(context_), "S: testS:  test again");
}

// Tests `GetContextString()` when the items overflow.
TEST_F(AILanguageModelContextTest, TestContextOperation_Overflow) {
  EXPECT_EQ(context_.AddContextItem(SimpleContextItem("test", 1u)),
            AILanguageModel::Context::SpaceReservationResult::kSufficientSpace);
  EXPECT_EQ(GetContextString(context_), "S: test");

  // Since the total number of tokens will exceed `kTestMaxContextToken`, the
  // old item will be evicted.
  EXPECT_EQ(
      context_.AddContextItem(
          SimpleContextItem("long token", kTestMaxContextToken)),
      AILanguageModel::Context::SpaceReservationResult::kSpaceMadeAvailable);
  EXPECT_EQ(GetContextString(context_), "S: long token");
}

TEST_F(AILanguageModelContextTest, TestContextOperation_PartialOverflow) {
  EXPECT_EQ(context_.AddContextItem(SimpleContextItem("foo", 1u)),
            AILanguageModel::Context::SpaceReservationResult::kSufficientSpace);
  EXPECT_EQ(GetContextString(context_), "S: foo");

  EXPECT_EQ(context_.AddContextItem(SimpleContextItem("bar", 1u)),
            AILanguageModel::Context::SpaceReservationResult::kSufficientSpace);
  EXPECT_EQ(GetContextString(context_), "S: fooS: bar");

  // Add 1 token less than `kTestMaxContextToken` so one of the previous items
  // will be kept.
  EXPECT_EQ(
      context_.AddContextItem(
          SimpleContextItem("long token", kTestMaxContextToken - 1)),
      AILanguageModel::Context::SpaceReservationResult::kSpaceMadeAvailable);
  EXPECT_EQ(GetContextString(context_), "S: barS: long token");
}

// Tests `GetContextString()` when the items overflow on the first insertion.
TEST_F(AILanguageModelContextTest, TestContextOperation_OverflowOnFirstItem) {
  EXPECT_EQ(
      context_.AddContextItem(
          SimpleContextItem("test very long token", kTestMaxContextToken + 1u)),
      AILanguageModel::Context::SpaceReservationResult::kInsufficientSpace);
  EXPECT_EQ(GetContextString(context_), "");
}

TEST_F(AILanguageModelTest, Priority) {
  fake_broker_->settings().set_execute_result({"hi"});
  auto session = CreateSession();

  EXPECT_THAT(Prompt(*session, MakeInput("foo")), ElementsAre("hi"));

  main_rfh()->GetRenderWidgetHost()->GetView()->Hide();
  EXPECT_THAT(Prompt(*session, MakeInput("bar")),
              ElementsAre("Priority: background", "hi"));

  auto fork = Fork(*session);
  EXPECT_THAT(Prompt(*fork, MakeInput("bar")),
              ElementsAre("Priority: background", "hi"));

  main_rfh()->GetRenderWidgetHost()->GetView()->Show();
  EXPECT_THAT(Prompt(*session, MakeInput("baz")), ElementsAre("hi"));
}

// Test that GetLanguageModelParams returns null when sampling config is
// not available (model not downloaded yet).
TEST_F(AILanguageModelTest, GetLanguageModelParamsReturnsNullWhenNotAvailable) {
  base::test::TestFuture<blink::mojom::AILanguageModelParamsPtr> future;
  ai_manager_->GetLanguageModelParams(future.GetCallback());

  EXPECT_FALSE(future.Get());
}

// Test that GetLanguageModelParams returns params when config is available
TEST_F(AILanguageModelTest,
       GetLanguageModelParamsReturnsValidParamsWhenAvailable) {
  EnsureModelIsReady();

  base::test::TestFuture<blink::mojom::AILanguageModelParamsPtr> future;
  ai_manager_->GetLanguageModelParams(future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  const auto& params = future.Get();

  ASSERT_TRUE(!params.is_null());
  ASSERT_TRUE(!params->default_sampling_params.is_null());
  EXPECT_EQ(kTestDefaultTopK, params->default_sampling_params->top_k);
  EXPECT_FLOAT_EQ(kTestDefaultTemperature,
                  params->default_sampling_params->temperature);
}

// Test class for `Tool Use` functionality.
class AILanguageModelOpenLoopToolTest : public AILanguageModelTest {
 protected:
  // Override CreateConfig to use higher max tokens for `Tool Use` testing.
  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig CreateConfig()
      override {
    auto config = AILanguageModelTest::CreateConfig();
    // Use higher max tokens to accommodate tool declarations/responses.
    config.mutable_input_config()->set_max_context_tokens(3000);
    return config;
  }

  // Helper to create a weather tool for testing.
  blink::mojom::AILanguageModelToolDeclarationPtr CreateWeatherTool() {
    auto tool = blink::mojom::AILanguageModelToolDeclaration::New();
    tool->name = "get_weather";
    tool->description = "Get current weather for a location";

    auto schema = base::JSONReader::ReadDict(R"({
      "type": "object",
      "properties": {
        "location": {
          "type": "string",
          "description": "City name"
        }
      },
      "required": ["location"]
    })",
                                             base::JSON_PARSE_RFC);
    CHECK(schema);
    tool->input_schema = std::move(*schema);
    return tool;
  }

  // Helper to configure simulated tool call.
  void SetupSimulatedToolCall(const std::string& call_id,
                              const std::string& tool_name,
                              const std::string& location) {
    base::DictValue args;
    args.Set("location", location);

    auto tool_call = on_device_model::mojom::ToolCall::New();
    tool_call->call_id = call_id;
    tool_call->name = tool_name;
    tool_call->arguments = std::move(args);

    std::vector<on_device_model::mojom::ToolCallPtr> simulated_calls;
    simulated_calls.push_back(std::move(tool_call));
    fake_broker_->settings().simulated_tool_calls = std::move(simulated_calls);
  }

  // Helper to create session with tools and system prompt.
  mojo::Remote<blink::mojom::AILanguageModel>
  CreateSessionWithToolsAndSystemPrompt(
      std::vector<blink::mojom::AILanguageModelToolDeclarationPtr> tools,
      const std::string& system_prompt = "You are a helpful assistant.") {
    auto options = blink::mojom::AILanguageModelCreateOptions::New();
    options->tools = std::move(tools);
    options->initial_prompts.push_back(
        MakePrompt(Role::kSystem, system_prompt));
    return CreateSession(std::move(options));
  }

  // Helper to setup tool call simulation and create session with tools.
  mojo::Remote<blink::mojom::AILanguageModel> SetupToolCallTestSession(
      const std::string& call_id,
      const std::string& location) {
    SetupSimulatedToolCall(call_id, "get_weather", location);

    std::vector<blink::mojom::AILanguageModelToolDeclarationPtr> tools;
    tools.push_back(CreateWeatherTool());
    return CreateSessionWithToolsAndSystemPrompt(std::move(tools));
  }

  // Helper to disable tool call simulation.
  void DisableToolCallSimulation() {
    fake_broker_->settings().simulated_tool_calls.clear();
  }
};

// Test that tools are embedded in the session's initial context.
TEST_F(AILanguageModelOpenLoopToolTest, ToolsEmbeddedInSystemPrompt) {
  std::vector<blink::mojom::AILanguageModelToolDeclarationPtr> tools;
  tools.push_back(CreateWeatherTool());
  auto session = CreateSessionWithToolsAndSystemPrompt(std::move(tools));

  // Verify tool declarations are passed to the on_device_model service.
  // The fake service echoes back its accumulated context, which includes tool
  // declarations formatted as "<tool name=...>".
  auto responses = Prompt(*session, MakeInput("Test"));
  ASSERT_THAT(responses, testing::SizeIs(testing::Ge(1)));

  std::string full_response = base::JoinString(responses, "");
  // Verify tool declaration content was embedded in context.
  EXPECT_THAT(full_response, testing::HasSubstr("<tool name=get_weather>"));
}

// Test that empty tools array doesn't break session creation or prompting.
TEST_F(AILanguageModelOpenLoopToolTest, EmptyToolsArrayHandled) {
  std::vector<blink::mojom::AILanguageModelToolDeclarationPtr> empty_tools;
  auto session = CreateSessionWithToolsAndSystemPrompt(std::move(empty_tools));

  // Session should work normally with empty tools array.
  auto responses = Prompt(*session, MakeInput("Hello"));
  ASSERT_THAT(responses, testing::SizeIs(testing::Ge(1)));

  // Verify the prompt was echoed back (no tool declarations should appear).
  std::string full_response = base::JoinString(responses, "");
  EXPECT_THAT(full_response, testing::HasSubstr("Hello"));
  EXPECT_THAT(full_response, testing::Not(testing::HasSubstr("<tool name=")));
}

// Test that tools without a system prompt are embedded in a synthetic system
// prompt.
TEST_F(AILanguageModelOpenLoopToolTest, ToolsEmbeddedWithoutSystemPrompt) {
  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  std::vector<blink::mojom::AILanguageModelToolDeclarationPtr> tools;
  tools.push_back(CreateWeatherTool());
  options->tools = std::move(tools);
  // No initial_prompts (no system prompt).
  auto session = CreateSession(std::move(options));

  auto responses = Prompt(*session, MakeInput("Test"));
  ASSERT_THAT(responses, testing::SizeIs(testing::Ge(1)));

  // Tool declarations should appear in the echo from a synthetic system prompt.
  std::string full_response = base::JoinString(responses, "");
  EXPECT_THAT(full_response, testing::HasSubstr("<tool name="));
}

// Test that multiple tools are properly embedded in system prompt.
TEST_F(AILanguageModelOpenLoopToolTest, MultipleToolsSupported) {
  std::vector<blink::mojom::AILanguageModelToolDeclarationPtr> tools;
  tools.push_back(CreateWeatherTool());

  // Add a calculator tool.
  auto calc_tool = blink::mojom::AILanguageModelToolDeclaration::New();
  calc_tool->name = "calculate";
  calc_tool->description = "Perform calculations";
  auto calc_schema = base::JSONReader::ReadDict(R"({
    "type": "object",
    "properties": {
      "expression": {
        "type": "string",
        "description": "Math expression"
      }
    },
    "required": ["expression"]
  })",
                                                base::JSON_PARSE_RFC);
  ASSERT_TRUE(calc_schema);
  calc_tool->input_schema = std::move(*calc_schema);
  tools.push_back(std::move(calc_tool));

  auto session = CreateSessionWithToolsAndSystemPrompt(std::move(tools));

  // Verify both tool declarations are passed to the on_device_model service.
  auto responses = Prompt(*session, MakeInput("Test"));
  ASSERT_THAT(responses, testing::SizeIs(testing::Ge(1)));

  std::string full_response = base::JoinString(responses, "");
  // Verify tool declarations were embedded in the context.
  EXPECT_THAT(full_response, testing::HasSubstr("<tool name=get_weather>"));
  EXPECT_THAT(full_response, testing::HasSubstr("<tool name=calculate>"));
}

// Test that OnToolCalls from the on_device_model service side is forwarded to
// Blink responder.
TEST_F(AILanguageModelOpenLoopToolTest, OnToolCallsForwardedToBlink) {
  auto session = SetupToolCallTestSession("call_123", "Seattle");

  // Execute prompt and verify tool calls are received.
  AITestUtils::TestStreamingResponder responder;
  session->Prompt(MakeInput("What's the weather?"), /*constraint=*/nullptr,
                  responder.BindRemote());

  // Wait for tool calls to be forwarded.
  EXPECT_TRUE(responder.WaitForToolCalls());
  ASSERT_EQ(responder.tool_calls().size(), 1u);
  EXPECT_EQ(responder.tool_calls()[0]->call_id, "call_123");
  EXPECT_EQ(responder.tool_calls()[0]->name, "get_weather");
}

// Test that tool response is handled.
TEST_F(AILanguageModelOpenLoopToolTest, ToolResponseHandled) {
  auto session = SetupToolCallTestSession("call_456", "Tokyo");

  // Get tool call.
  AITestUtils::TestStreamingResponder responder1;
  session->Prompt(MakeInput("Weather in Tokyo?"), /*constraint=*/nullptr,
                  responder1.BindRemote());
  EXPECT_TRUE(responder1.WaitForToolCalls());

  // Disable tool simulation.
  DisableToolCallSimulation();

  // Send structured tool response with nested objects/arrays.
  base::DictValue tool_response_dict;
  tool_response_dict.Set("callID", "call_456");
  tool_response_dict.Set("name", "get_weather");

  base::DictValue result;
  result.Set("temperature", 28);
  result.Set("condition", "Rainy");
  result.Set("humidity", 85);

  base::ListValue forecast;
  base::DictValue day1;
  day1.Set("day", "today");
  day1.Set("high", 30);
  day1.Set("low", 25);
  forecast.Append(std::move(day1));

  base::DictValue day2;
  day2.Set("day", "tomorrow");
  day2.Set("high", 32);
  day2.Set("low", 27);
  forecast.Append(std::move(day2));

  result.Set("forecast", std::move(forecast));

  base::ListValue alerts;
  alerts.Append("Heavy rain warning");
  result.Set("alerts", std::move(alerts));

  tool_response_dict.Set("result", std::move(result));

  // Create prompt with tool response as structured content with kUser role.
  std::vector<blink::mojom::AILanguageModelPromptPtr> tool_response_prompts;
  auto prompt = blink::mojom::AILanguageModelPrompt::New();
  prompt->role = blink::mojom::AILanguageModelPromptRole::kUser;
  prompt->content.push_back(
      blink::mojom::AILanguageModelPromptContent::NewToolResponse(
          std::move(tool_response_dict)));
  tool_response_prompts.push_back(std::move(prompt));

  // Verify model receives and processes structured data.
  AITestUtils::TestStreamingResponder responder2;
  session->Prompt(std::move(tool_response_prompts), /*constraint=*/nullptr,
                  responder2.BindRemote());
  EXPECT_TRUE(responder2.WaitForCompletion());

  std::string response = base::JoinString(responder2.responses(), "");
  // The fake service echoes back its accumulated context. The context includes
  // the ToolResponse formatted by fake_service.cc's OnDeviceInputToString().
  // Verify that tool response was properly formatted.
  EXPECT_THAT(response, testing::HasSubstr(
                            "<tool-response id=call_456 name=get_weather"));
  // Verify the result JSON was serialized and included.
  EXPECT_THAT(response, testing::HasSubstr("\"temperature\":28"));
  EXPECT_THAT(response, testing::HasSubstr("\"condition\":\"Rainy\""));
  EXPECT_THAT(response, testing::HasSubstr("\"humidity\":85"));
  // Verify nested arrays in the result JSON.
  EXPECT_THAT(response, testing::HasSubstr("\"forecast\":["));
  EXPECT_THAT(response, testing::HasSubstr("\"day\":\"today\""));
  EXPECT_THAT(response, testing::HasSubstr("\"day\":\"tomorrow\""));
  EXPECT_THAT(response, testing::HasSubstr("\"high\":30"));
  EXPECT_THAT(response, testing::HasSubstr("\"high\":32"));
  // Verify alerts array.
  EXPECT_THAT(response,
              testing::HasSubstr("\"alerts\":[\"Heavy rain warning\"]"));
}

// Test that tool response with error is handled.
TEST_F(AILanguageModelOpenLoopToolTest, ToolResponseWithError) {
  auto session = SetupToolCallTestSession("call_789", "InvalidCity");

  // Get tool call.
  AITestUtils::TestStreamingResponder responder1;
  session->Prompt(MakeInput("Weather in InvalidCity?"), /*constraint=*/nullptr,
                  responder1.BindRemote());
  EXPECT_TRUE(responder1.WaitForToolCalls());

  // Disable simulation.
  DisableToolCallSimulation();

  // Send tool error response.
  base::DictValue error_response_dict;
  error_response_dict.Set("callID", "call_789");
  error_response_dict.Set("name", "get_weather");
  error_response_dict.Set("errorMessage",
                          "Unable to find weather data for InvalidCity");

  std::vector<blink::mojom::AILanguageModelPromptPtr> error_prompts;
  auto prompt = blink::mojom::AILanguageModelPrompt::New();
  prompt->role = blink::mojom::AILanguageModelPromptRole::kUser;
  prompt->content.push_back(
      blink::mojom::AILanguageModelPromptContent::NewToolResponse(
          std::move(error_response_dict)));
  error_prompts.push_back(std::move(prompt));

  // Verify model handles error gracefully.
  AITestUtils::TestStreamingResponder responder2;
  session->Prompt(std::move(error_prompts), /*constraint=*/nullptr,
                  responder2.BindRemote());
  EXPECT_TRUE(responder2.WaitForCompletion());

  std::string response = base::JoinString(responder2.responses(), "");
  // The fake service echoes back its accumulated context. The context includes
  // the ToolResponse formatted by fake_service.cc's OnDeviceInputToString().
  // Verify that tool response error was properly formatted.
  EXPECT_THAT(response, testing::HasSubstr(
                            "<tool-response id=call_789 name=get_weather"));
  // Verify error message was included (not result JSON).
  EXPECT_THAT(response,
              testing::HasSubstr(
                  "error=\"Unable to find weather data for InvalidCity\""));
  EXPECT_THAT(response, testing::Not(testing::HasSubstr("result=")));
}

// Test complete open-loop tool call flow.
TEST_F(AILanguageModelOpenLoopToolTest, CompleteOpenLoopToolCallFlow) {
  // ========== STEP 1: Setup tool call simulation ==========
  auto session = SetupToolCallTestSession("call_123", "Seattle");

  // ========== STEP 2: Send initial prompt, receive tool call ==========
  AITestUtils::TestStreamingResponder responder1;
  session->Prompt(MakeInput("What's the weather in Seattle?"),
                  /*constraint=*/nullptr, responder1.BindRemote());

  EXPECT_TRUE(responder1.WaitForToolCalls());
  ASSERT_EQ(responder1.tool_calls().size(), 1u);
  EXPECT_EQ(responder1.tool_calls()[0]->call_id, "call_123");
  EXPECT_EQ(responder1.tool_calls()[0]->name, "get_weather");

  // Tool call arguments should match what we expect.
  const std::string* location =
      responder1.tool_calls()[0]->arguments.FindString("location");
  ASSERT_TRUE(location);
  EXPECT_EQ(*location, "Seattle");

  // ========== STEP 3: Disable tool simulation for next prompt ==========
  // Important! Otherwise the model will trigger tools again.
  DisableToolCallSimulation();

  // ========== STEP 4: Create and send tool response as new prompt ==========
  // Compose the result for ToolResponse.
  base::DictValue tool_result_dict;
  tool_result_dict.Set("callID", "call_123");
  tool_result_dict.Set("name", "get_weather");

  base::DictValue result;
  result.Set("temperature", 72);
  result.Set("condition", "Sunny");
  result.Set("location", "Seattle");
  tool_result_dict.Set("result", std::move(result));

  std::vector<blink::mojom::AILanguageModelPromptPtr> tool_result_prompts;
  auto tool_prompt = blink::mojom::AILanguageModelPrompt::New();
  tool_prompt->role = blink::mojom::AILanguageModelPromptRole::kUser;
  tool_prompt->content.push_back(
      blink::mojom::AILanguageModelPromptContent::NewToolResponse(
          std::move(tool_result_dict)));
  tool_result_prompts.push_back(std::move(tool_prompt));

  // ========== STEP 5: Send tool result and verify final response ==========
  AITestUtils::TestStreamingResponder responder2;
  session->Prompt(std::move(tool_result_prompts), /*constraint=*/nullptr,
                  responder2.BindRemote());

  // Should receive completion, NOT another tool call.
  EXPECT_TRUE(responder2.WaitForCompletion());
  EXPECT_EQ(responder2.tool_calls().size(), 0u);  // No more tool calls.

  // ========== STEP 6: Verify final response uses tool result ==========
  ASSERT_GT(responder2.responses().size(), 0u);
  std::string final_response = base::JoinString(responder2.responses(), "");

  // The fake service echoes back its accumulated context. The context includes
  // the ToolResponse formatted by fake_service.cc's OnDeviceInputToString().
  // Verify that tool response was properly formatted.
  EXPECT_THAT(
      final_response,
      testing::HasSubstr("<tool-response id=call_123 name=get_weather"));
  // Verify the result JSON was serialized and included.
  EXPECT_THAT(final_response, testing::HasSubstr("\"temperature\":72"));
  EXPECT_THAT(final_response, testing::HasSubstr("\"condition\":\"Sunny\""));
  EXPECT_THAT(final_response, testing::HasSubstr("\"location\":\"Seattle\""));
}

// Test that cloned sessions preserve tools and tool call functionality.
TEST_F(AILanguageModelOpenLoopToolTest, ClonedSessionPreservesTools) {
  // ========== STEP 1: Create original session with tools ==========
  SetupSimulatedToolCall("call_clone_001", "get_weather", "Paris");

  std::vector<blink::mojom::AILanguageModelToolDeclarationPtr> tools;
  tools.push_back(CreateWeatherTool());
  auto session = CreateSessionWithToolsAndSystemPrompt(std::move(tools));

  // ========== STEP 2: Send prompt to original session ==========
  AITestUtils::TestStreamingResponder responder1;
  session->Prompt(MakeInput("What's the weather in Paris?"),
                  /*constraint=*/nullptr, responder1.BindRemote());
  EXPECT_TRUE(responder1.WaitForToolCalls());
  ASSERT_EQ(responder1.tool_calls().size(), 1u);
  EXPECT_EQ(responder1.tool_calls()[0]->call_id, "call_clone_001");
  EXPECT_EQ(responder1.tool_calls()[0]->name, "get_weather");

  // ========== STEP 3: Clone the session ==========
  mojo::Remote<blink::mojom::AILanguageModel> cloned_session = Fork(*session);

  // ========== STEP 4: Verify tool calls work in cloned session ==========
  // Configure new tool call for cloned session.
  SetupSimulatedToolCall("call_clone_002", "get_weather", "London");

  AITestUtils::TestStreamingResponder responder2;
  cloned_session->Prompt(MakeInput("What's the weather in London?"),
                         /*constraint=*/nullptr, responder2.BindRemote());
  EXPECT_TRUE(responder2.WaitForToolCalls());
  ASSERT_EQ(responder2.tool_calls().size(), 1u);
  EXPECT_EQ(responder2.tool_calls()[0]->call_id, "call_clone_002");
  EXPECT_EQ(responder2.tool_calls()[0]->name, "get_weather");

  // ========== STEP 5: Verify tool responses work in cloned session ==========
  // Disable tool simulation.
  DisableToolCallSimulation();

  // Send tool response to cloned session.
  base::DictValue tool_response_dict;
  tool_response_dict.Set("callID", "call_clone_002");
  tool_response_dict.Set("name", "get_weather");

  base::DictValue result;
  result.Set("temperature", 18);
  result.Set("condition", "Cloudy");
  tool_response_dict.Set("result", std::move(result));

  std::vector<blink::mojom::AILanguageModelPromptPtr> tool_result_prompts;
  auto tool_prompt = blink::mojom::AILanguageModelPrompt::New();
  tool_prompt->role = blink::mojom::AILanguageModelPromptRole::kUser;
  tool_prompt->content.push_back(
      blink::mojom::AILanguageModelPromptContent::NewToolResponse(
          std::move(tool_response_dict)));
  tool_result_prompts.push_back(std::move(tool_prompt));

  AITestUtils::TestStreamingResponder responder3;
  cloned_session->Prompt(std::move(tool_result_prompts),
                         /*constraint=*/nullptr, responder3.BindRemote());
  EXPECT_TRUE(responder3.WaitForCompletion());

  // Verify tool response was properly formatted in cloned session.
  std::string final_response = base::JoinString(responder3.responses(), "");
  EXPECT_THAT(
      final_response,
      testing::HasSubstr("<tool-response id=call_clone_002 name=get_weather"));
  EXPECT_THAT(final_response, testing::HasSubstr("\"temperature\":18"));
  EXPECT_THAT(final_response, testing::HasSubstr("\"condition\":\"Cloudy\""));
}

TEST_F(AILanguageModelTest, CanCreatePermissionsPolicyDisabled) {
  DisablePolicy(network::mojom::PermissionsPolicyFeature::kLanguageModel);
  mojo::test::BadMessageObserver observer;
  GetAIManagerRemote()->CanCreateLanguageModel(
      blink::mojom::AILanguageModelCreateOptions::New(), base::DoNothing());
  EXPECT_EQ(observer.WaitForBadMessage(), "Permissions policy disabled");
}

TEST_F(AILanguageModelTest, CreatePermissionsPolicyDisabled) {
  DisablePolicy(network::mojom::PermissionsPolicyFeature::kLanguageModel);
  mojo::test::BadMessageObserver observer;
  TestCreateLanguageModelClient language_model_client;
  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(),
      blink::mojom::AILanguageModelCreateOptions::New());
  EXPECT_EQ(observer.WaitForBadMessage(), "Policy or user setting disabled");
}

TEST_F(AILanguageModelTest, CreateBuiltInAIAPIsEnterprisePolicyDisabled) {
  SetBuiltInAIAPIsEnterprisePolicy(false);
  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  ai_manager_->CanCreateLanguageModel(
      blink::mojom::AILanguageModelCreateOptions::New(), future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                              kUnavailableEnterprisePolicyDisabled);

  mojo::test::BadMessageObserver observer;
  TestCreateLanguageModelClient language_model_client;
  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(),
      blink::mojom::AILanguageModelCreateOptions::New());
  EXPECT_EQ(observer.WaitForBadMessage(), "Policy or user setting disabled");
  SetBuiltInAIAPIsEnterprisePolicy(true);
}

TEST_F(AILanguageModelTest, CreateGenAILocalEnterprisePolicyDisabled) {
  SetGenAILocalEnterprisePolicy(false);
  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  ai_manager_->CanCreateLanguageModel(
      blink::mojom::AILanguageModelCreateOptions::New(), future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                              kUnavailableEnterprisePolicyDisabled);

  mojo::test::BadMessageObserver observer;
  TestCreateLanguageModelClient language_model_client;
  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(),
      blink::mojom::AILanguageModelCreateOptions::New());
  EXPECT_EQ(observer.WaitForBadMessage(), "Policy or user setting disabled");
  SetGenAILocalEnterprisePolicy(true);
}

TEST_F(AILanguageModelTest, CreateOnDeviceAiUserSettingDisabled) {
  SetOnDeviceAiUserSetting(false);
  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  ai_manager_->CanCreateLanguageModel(
      blink::mojom::AILanguageModelCreateOptions::New(), future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                              kUnavailableFeatureNotEnabled);

  mojo::test::BadMessageObserver observer;
  TestCreateLanguageModelClient language_model_client;
  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(),
      blink::mojom::AILanguageModelCreateOptions::New());
  EXPECT_EQ(observer.WaitForBadMessage(), "Policy or user setting disabled");
  SetOnDeviceAiUserSetting(true);
}

class AILanguageModelConfiguredMaxOutputTokensTest
    : public AILanguageModelTest {
 protected:
  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig CreateConfig()
      override {
    auto config = AILanguageModelTest::CreateConfig();
    config.mutable_output_config()->set_max_output_tokens(
        kTestConfiguredMaxOutputTokens);
    return config;
  }
};

TEST_F(AILanguageModelConfiguredMaxOutputTokensTest,
       OutputCappedByConfiguredMaxOutputTokens) {
  auto session = CreateSession();
  // Set a fake response that exceeds the configured max_output_tokens (10) but
  // is well within the context-window capacity. Without the cap, this would
  // succeed; with the cap it should trigger kErrorResponseExceedsMaxTokens.
  fake_broker_->settings().set_execute_result({std::string(15, 'a')});
  AITestUtils::TestStreamingResponder responder;
  session->Prompt(MakeInput("foo"), nullptr, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::
                kErrorResponseExceedsMaxTokens);
}

}  // namespace

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_rewriter.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "chrome/browser/ai/features.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/fake_manifest_broker.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/scenario_builder.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/mock_on_device_capability.h"
#include "components/optimization_guide/core/model_execution/test/substitution_builder.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/feature_configs.pb.h"
#include "components/optimization_guide/proto/features/writing_assistance_api.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "services/on_device_model/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace {

namespace proto = ::optimization_guide::proto;

using ::base::test::TestFuture;
using ::blink::mojom::AILanguageCode;
using ::blink::mojom::AILanguageCodePtr;
using ::on_device_model::mojom::PerformanceClass;
using ::optimization_guide::FieldSubstitution;
using ::optimization_guide::ForbidUnsafe;
using ::optimization_guide::ProtoField;
using ::optimization_guide::StringValueField;
using ::optimization_guide::proto::WritingAssistanceApiRequest;
using ::optimization_guide::proto::WritingAssistanceApiResponse;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

constexpr char kSharedContextString[] = "test shared context";
constexpr char kContextString[] = "test context";
constexpr char kInputString[] = "input string";

struct Error {
  blink::mojom::AIManagerCreateClientError error;
  blink::mojom::QuotaErrorInfoPtr quota_error_info;
};

using CreateRewriterResult =
    base::expected<mojo::PendingRemote<blink::mojom::AIRewriter>, Error>;

class TestCreateRewriterClient
    : public blink::mojom::AIManagerCreateRewriterClient {
 public:
  TestCreateRewriterClient() = default;
  ~TestCreateRewriterClient() override = default;
  TestCreateRewriterClient(const TestCreateRewriterClient&) = delete;
  TestCreateRewriterClient& operator=(const TestCreateRewriterClient&) = delete;

  mojo::PendingRemote<blink::mojom::AIManagerCreateRewriterClient>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnResult(mojo::PendingRemote<::blink::mojom::AIRewriter> rewriter,
                uint64_t context_window) override {
    context_window_ = context_window;
    result_.SetValue(std::move(rewriter));
  }

  void OnError(blink::mojom::AIManagerCreateClientError error,
               blink::mojom::QuotaErrorInfoPtr quota_error_info) override {
    result_.SetValue(
        base::unexpected(Error{error, std::move(quota_error_info)}));
  }

  TestFuture<CreateRewriterResult>& result() { return result_; }
  uint64_t context_window() const { return context_window_; }

 private:
  TestFuture<CreateRewriterResult> result_;
  uint64_t context_window_ = 0;
  mojo::Receiver<blink::mojom::AIManagerCreateRewriterClient> receiver_{this};
};

blink::mojom::AIRewriterCreateOptionsPtr GetDefaultOptions() {
  return blink::mojom::AIRewriterCreateOptions::New(
      kSharedContextString, blink::mojom::AIRewriterTone::kAsIs,
      blink::mojom::AIRewriterFormat::kAsIs,
      blink::mojom::AIRewriterLength::kAsIs,
      /*expected_input_languages=*/std::vector<AILanguageCodePtr>(),
      /*expected_context_languages=*/std::vector<AILanguageCodePtr>(),
      /*output_language=*/AILanguageCode::New(""));
}

optimization_guide::proto::FeatureTextSafetyConfiguration CreateSafetyConfig() {
  optimization_guide::proto::FeatureTextSafetyConfiguration safety_config;
  safety_config.set_feature(optimization_guide::proto::
                                MODEL_EXECUTION_FEATURE_WRITING_ASSISTANCE_API);
  safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
  {
    auto* check = safety_config.add_request_check();
    check->mutable_input_template()->Add(FieldSubstitution(
        "%s", ProtoField({WritingAssistanceApiRequest::kContextFieldNumber})));
  }
  {
    auto* check = safety_config.add_request_check();
    check->mutable_input_template()->Add(FieldSubstitution(
        "%s",
        ProtoField({WritingAssistanceApiRequest::kSharedContextFieldNumber})));
  }
  {
    auto* check = safety_config.add_request_check();
    check->mutable_input_template()->Add(FieldSubstitution(
        "%s",
        ProtoField({WritingAssistanceApiRequest::kRewriteTextFieldNumber})));
  }

  return safety_config;
}

optimization_guide::proto::OnDeviceModelExecutionFeatureConfig
CreateRewriterConfig() {
  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig config;
  config.set_can_skip_text_safety(true);
  config.set_feature(optimization_guide::proto::ModelExecutionFeature::
                         MODEL_EXECUTION_FEATURE_WRITING_ASSISTANCE_API);

  auto& input_config = *config.mutable_input_config();
  input_config.set_request_base_name(
      WritingAssistanceApiRequest().GetTypeName());
  input_config.set_max_execute_tokens(
      blink::mojom::kWritingAssistanceMaxInputTokenSize);

  *input_config.add_execute_substitutions() = FieldSubstitution(
      "%s", ProtoField({WritingAssistanceApiRequest::kContextFieldNumber}));
  *input_config.add_execute_substitutions() = FieldSubstitution(
      "%s",
      ProtoField({WritingAssistanceApiRequest::kSharedContextFieldNumber}));
  *input_config.add_execute_substitutions() = FieldSubstitution(
      "%s", ProtoField({WritingAssistanceApiRequest::kRewriteTextFieldNumber}));

  auto& output_config = *config.mutable_output_config();
  output_config.set_proto_type(WritingAssistanceApiResponse().GetTypeName());
  *output_config.mutable_proto_field() = StringValueField();

  return config;
}

class AIRewriterTest : public AITestUtils::AITestBase {
 public:
  AIRewriterTest() {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kAIRewriterAPI);
  }

 protected:
  proto::SolutionConfig CreateSolution() override {
    proto::SolutionConfig solution_config;
    *solution_config.mutable_feature() = CreateRewriterConfig();
    *solution_config.mutable_safety() = CreateSafetyConfig();
    return solution_config;
  }

  mojo::Remote<blink::mojom::AIRewriter> GetAIRewriterRemote(
      blink::mojom::AIRewriterCreateOptionsPtr options = GetDefaultOptions()) {
    TestCreateRewriterClient create_rewriter_client;
    GetAIManagerRemote()->CreateRewriter(
        create_rewriter_client.BindNewPipeAndPassRemote(), std::move(options),
        /*monitor=*/mojo::NullRemote());

    CreateRewriterResult result = create_rewriter_client.result().Take();
    EXPECT_OK(result);
    return mojo::Remote<blink::mojom::AIRewriter>(std::move(result.value()));
  }

  void RunSimpleRewriteTest(blink::mojom::AIRewriterTone tone,
                            blink::mojom::AIRewriterFormat format,
                            blink::mojom::AIRewriterLength length) {
    fake_broker_->settings().set_execute_result({"Result text"});

    const auto options = blink::mojom::AIRewriterCreateOptions::New(
        kSharedContextString, tone, format, length,
        /*expected_input_languages=*/std::vector<AILanguageCodePtr>(),
        /*expected_context_languages=*/std::vector<AILanguageCodePtr>(),
        /*output_language=*/AILanguageCode::New(""));

    mojo::Remote<blink::mojom::AIRewriter> rewriter_remote =
        GetAIRewriterRemote(options.Clone());

    EXPECT_THAT(Rewrite(*rewriter_remote, kInputString, kContextString),
                ElementsAreArray({"Result text"}));
  }

  std::vector<std::string> Rewrite(blink::mojom::AIRewriter& rewriter,
                                   const std::string& input,
                                   const std::string& context) {
    AITestUtils::TestStreamingResponder responder;
    rewriter.Rewrite(kInputString, kContextString, responder.BindRemote());
    EXPECT_TRUE(responder.WaitForCompletion());
    // Return Rewrite's response without the final empty string chunk.
    return responder.responses_without_last();
  }

  void EnsureModelIsReady() {
    TestCreateRewriterClient rewriter_client;
    GetAIManagerRemote()->CreateRewriter(
        rewriter_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
        /*monitor=*/mojo::NullRemote());

    auto result = rewriter_client.result().Take();
    EXPECT_OK(result);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AIRewriterTest, CreateRewriterNoService) {
  SetupNullOptimizationGuideKeyedService();

  TestCreateRewriterClient create_rewriter_client;
  GetAIManagerRemote()->CreateRewriter(
      create_rewriter_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());

  CreateRewriterResult result = create_rewriter_client.result().Take();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

TEST_F(AIRewriterTest, RewriterTelemetry) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      GetOnDeviceModelEligibility(
          optimization_guide::mojom::OnDeviceFeature::kWritingAssistanceApi))
      .WillRepeatedly(testing::Return(
          optimization_guide::OnDeviceModelEligibilityReason::kSuccess));
  EnsureModelIsReady();
  GetAIRewriterRemote();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution."
      "OnDeviceModelEligibilityReason.WritingAssistanceApi",
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess, 2);
}

TEST_F(AIRewriterTest, CreateRewriterModelNotEligible) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{optimization_guide::features::kOnDeviceModelPerformanceParams,
        {{"compatible_on_device_performance_classes", "3,4,5,6"}}}},
      {{on_device_model::features::kOnDeviceModelCpuBackend}});

  fake_broker_->settings().performance_class =
      on_device_model::mojom::PerformanceClass::kVeryLow;

  TestCreateRewriterClient create_rewriter_client;
  GetAIManagerRemote()->CreateRewriter(
      create_rewriter_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());

  CreateRewriterResult result = create_rewriter_client.result().Take();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(AIRewriterTest, CreateRewriterSafetyConfigNotAvailable) {
  SetSolutionConfig([&]() {
    auto solution_config = CreateSolution();
    solution_config.mutable_feature()->set_can_skip_text_safety(false);
    // Provide a safety asset that does not support rewriter.
    solution_config.mutable_safety()->set_feature(
        optimization_guide::proto::MODEL_EXECUTION_FEATURE_TEST);
    return solution_config;
  }());

  TestCreateRewriterClient create_rewriter_client;
  GetAIManagerRemote()->CreateRewriter(
      create_rewriter_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());

  CreateRewriterResult result = create_rewriter_client.result().Take();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}
#endif

TEST_F(AIRewriterTest, CreateRewriterUnableToCalculateTokenSize) {
  // Incorrect `request_base_name` cause session to fail constructing input
  // string and checking token size.
  SetSolutionConfig([&]() {
    auto solution_config = CreateSolution();
    solution_config.mutable_feature()
        ->mutable_input_config()
        ->set_request_base_name("InvalidRequestBaseName");
    return solution_config;
  }());

  TestCreateRewriterClient create_rewriter_client;
  GetAIManagerRemote()->CreateRewriter(
      create_rewriter_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());

  CreateRewriterResult result = create_rewriter_client.result().Take();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(
      result.error().error,
      blink::mojom::AIManagerCreateClientError::kUnableToCalculateTokenSize);
}

TEST_F(AIRewriterTest, CreateRewriterContextLimitExceededError) {
  fake_broker_->settings().set_size_in_tokens(
      blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);

  TestCreateRewriterClient create_rewriter_client;
  GetAIManagerRemote()->CreateRewriter(
      create_rewriter_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());

  CreateRewriterResult result = create_rewriter_client.result().Take();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kInitialInputTooLarge);
  EXPECT_EQ(result.error().quota_error_info->requested,
            blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);
  EXPECT_EQ(result.error().quota_error_info->quota,
            blink::mojom::kWritingAssistanceMaxInputTokenSize);
}

TEST_F(AIRewriterTest, ContextWindowUsesContextLimit) {
  TestCreateRewriterClient client;
  GetAIManagerRemote()->CreateRewriter(client.BindNewPipeAndPassRemote(),
                                       GetDefaultOptions(),
                                       /*monitor=*/mojo::NullRemote());
  CreateRewriterResult result = client.result().Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(client.context_window(),
            blink::mojom::kWritingAssistanceMaxInputTokenSize);
}

TEST_F(AIRewriterTest, CustomInputContextLimit) {
  constexpr uint32_t kCustomMaxTokens = 5000;
  SetModelInputContextLimit(kCustomMaxTokens);

  TestCreateRewriterClient client;
  GetAIManagerRemote()->CreateRewriter(client.BindNewPipeAndPassRemote(),
                                       GetDefaultOptions(),
                                       /*monitor=*/mojo::NullRemote());
  CreateRewriterResult result = client.result().Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(client.context_window(), kCustomMaxTokens);

  SetSizeInTokens(kCustomMaxTokens + 1);

  TestCreateRewriterClient create_client;
  GetAIManagerRemote()->CreateRewriter(
      create_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());
  CreateRewriterResult create_result = create_client.result().Take();
  EXPECT_FALSE(create_result.has_value());
  EXPECT_EQ(create_result.error().error,
            blink::mojom::AIManagerCreateClientError::kInitialInputTooLarge);
  EXPECT_EQ(create_result.error().quota_error_info->requested,
            kCustomMaxTokens + 1);
  EXPECT_EQ(create_result.error().quota_error_info->quota, kCustomMaxTokens);

  mojo::Remote<blink::mojom::AIRewriter> rewriter_remote(
      std::move(result.value()));
  AITestUtils::TestStreamingResponder responder;
  rewriter_remote->Rewrite(kInputString, kContextString, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorInputTooLarge);
  ASSERT_EQ(responder.quota_error_info().requested, kCustomMaxTokens + 1);
  ASSERT_EQ(responder.quota_error_info().quota, kCustomMaxTokens);
}

TEST_F(AIRewriterTest, CanCreateDefaultOptions) {
  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    GetAIManagerInterface()->CanCreateRewriter(GetDefaultOptions(),
                                               future.GetCallback());
    EXPECT_EQ(future.Get(),
              blink::mojom::ModelAvailabilityCheckResult::kDownloadable);
  }

  // After model is ready, `CanCreateRewriter` should return available.
  EnsureModelIsReady();

  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    GetAIManagerInterface()->CanCreateRewriter(GetDefaultOptions(),
                                               future.GetCallback());
    EXPECT_EQ(future.Get(),
              blink::mojom::ModelAvailabilityCheckResult::kAvailable);
  }
}

TEST_F(AIRewriterTest, CanCreateIsLanguagesSupported) {
  EnsureModelIsReady();

  auto options = GetDefaultOptions();
  options->output_language = AILanguageCode::New("en");
  options->expected_input_languages =
      AITestUtils::ToMojoLanguageCodes({"en-US", ""});
  options->expected_context_languages =
      AITestUtils::ToMojoLanguageCodes({"en-GB", ""});

  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  GetAIManagerInterface()->CanCreateRewriter(std::move(options),
                                             future.GetCallback());
  EXPECT_EQ(future.Get(),
            blink::mojom::ModelAvailabilityCheckResult::kAvailable);
}

TEST_F(AIRewriterTest, CanCreateUnIsLanguagesSupported) {
  auto options = GetDefaultOptions();
  options->output_language = AILanguageCode::New("es-ES");
  options->expected_input_languages =
      AITestUtils::ToMojoLanguageCodes({"en", "tlh", "ja"});
  options->expected_context_languages =
      AITestUtils::ToMojoLanguageCodes({"ar", "zh", "hi"});
  base::MockCallback<AIManager::CanCreateRewriterCallback> callback;
  EXPECT_CALL(callback, Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage));
  GetAIManagerInterface()->CanCreateRewriter(std::move(options),
                                             callback.Get());
}

TEST_F(AIRewriterTest, ToProtoOptionsLanguagesSupported) {
  // Rewriter proto expects base language display names in English.
  std::vector<std::pair<std::string, std::string>> languages = {
      {"en", "English"},  {"en-us", "English"},  {"en-gb", "English"},
      {"es", "Spanish"},  {"es-es", "Spanish"},  {"es-mx", "Spanish"},
      {"ja", "Japanese"}, {"ja-jp", "Japanese"},
  };
  blink::mojom::AIRewriterCreateOptionsPtr options = GetDefaultOptions();
  for (const auto& language : languages) {
    options->output_language = AILanguageCode::New(language.first);
    const auto proto_options = AIRewriter::ToProtoOptions(options);
    EXPECT_EQ(proto_options->output_language(), language.second);
  }
}

TEST_F(AIRewriterTest, RewriteDefault) {
  RunSimpleRewriteTest(blink::mojom::AIRewriterTone::kAsIs,
                       blink::mojom::AIRewriterFormat::kAsIs,
                       blink::mojom::AIRewriterLength::kAsIs);
}

TEST_F(AIRewriterTest, RewriteWithOptions) {
  blink::mojom::AIRewriterTone tones[]{
      blink::mojom::AIRewriterTone::kAsIs,
      blink::mojom::AIRewriterTone::kMoreFormal,
      blink::mojom::AIRewriterTone::kMoreCasual,
  };
  blink::mojom::AIRewriterFormat formats[]{
      blink::mojom::AIRewriterFormat::kAsIs,
      blink::mojom::AIRewriterFormat::kPlainText,
      blink::mojom::AIRewriterFormat::kMarkdown,
  };
  blink::mojom::AIRewriterLength lengths[]{
      blink::mojom::AIRewriterLength::kAsIs,
      blink::mojom::AIRewriterLength::kShorter,
      blink::mojom::AIRewriterLength::kLonger,
  };
  for (const auto& tone : tones) {
    for (const auto& format : formats) {
      for (const auto& length : lengths) {
        SCOPED_TRACE(testing::Message()
                     << tone << " " << format << " " << length);
        RunSimpleRewriteTest(tone, format, length);
      }
    }
  }
}

TEST_F(AIRewriterTest, InputLimitExceededError) {
  auto rewriter_remote = GetAIRewriterRemote();

  fake_broker_->settings().set_size_in_tokens(
      blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);

  AITestUtils::TestStreamingResponder responder;
  rewriter_remote->Rewrite(kInputString, kContextString,
                           responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorInputTooLarge);
  ASSERT_EQ(responder.quota_error_info().requested,
            blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);
  ASSERT_EQ(responder.quota_error_info().quota,
            blink::mojom::kWritingAssistanceMaxInputTokenSize);
}

TEST_F(AIRewriterTest, RewriteMultipleResponse) {
  auto rewriter_remote = GetAIRewriterRemote();

  std::vector<std::string> result = {"Result ", "text"};
  fake_broker_->settings().set_execute_result(result);
  EXPECT_THAT(Rewrite(*rewriter_remote, kInputString, kContextString),
              ElementsAreArray(result));
}

TEST_F(AIRewriterTest, MultipleRewrite) {
  auto rewriter_remote = GetAIRewriterRemote();

  std::vector<std::string> result = {"Result ", "text"};
  fake_broker_->settings().set_execute_result(result);
  EXPECT_THAT(Rewrite(*rewriter_remote, kInputString, kContextString),
              ElementsAreArray(result));

  std::vector<std::string> result2 = {"Result ", "text ", "2"};
  fake_broker_->settings().set_execute_result(result2);
  EXPECT_THAT(Rewrite(*rewriter_remote, "input string 2", "test context 2"),
              ElementsAreArray(result2));
}

TEST_F(AIRewriterTest, MeasureUsage) {
  auto rewriter_remote = GetAIRewriterRemote();

  base::test::TestFuture<std::optional<uint32_t>> future;
  rewriter_remote->MeasureUsage(kInputString, kContextString,
                                future.GetCallback());

  auto size = std::string(kSharedContextString).size() +
              std::string(kContextString).size() +
              std::string(kInputString).size();
  EXPECT_EQ(future.Get(), size);
}

TEST_F(AIRewriterTest, Priority) {
  fake_broker_->settings().set_execute_result({"hi"});
  auto rewriter_remote = GetAIRewriterRemote();

  EXPECT_THAT(Rewrite(*rewriter_remote, kInputString, kContextString),
              ElementsAre("hi"));

  web_contents()->WasHidden();
  EXPECT_THAT(Rewrite(*rewriter_remote, kInputString, kContextString),
              ElementsAre("Priority: background", "hi"));

  web_contents()->WasShown();
  EXPECT_THAT(Rewrite(*rewriter_remote, kInputString, kContextString),
              ElementsAre("hi"));
}

TEST_F(AIRewriterTest, TextSafetyInput) {
  SetSolutionConfig([&]() {
    auto solution_config = CreateSolution();
    solution_config.mutable_feature()->set_can_skip_text_safety(false);
    return solution_config;
  }());

  fake_broker_->settings().set_execute_result({"hi"});
  auto rewriter_remote = GetAIRewriterRemote();
  EXPECT_THAT(Rewrite(*rewriter_remote, kInputString, kContextString),
              ElementsAre("hi"));

  AITestUtils::TestStreamingResponder responder;
  rewriter_remote->Rewrite("unsafe", kContextString, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
}

TEST_F(AIRewriterTest, TextSafetyContext) {
  SetSolutionConfig([&]() {
    auto solution_config = CreateSolution();
    solution_config.mutable_feature()->set_can_skip_text_safety(false);
    return solution_config;
  }());

  fake_broker_->settings().set_execute_result({"hi"});
  auto rewriter_remote = GetAIRewriterRemote();
  EXPECT_THAT(Rewrite(*rewriter_remote, kInputString, kContextString),
              ElementsAre("hi"));

  AITestUtils::TestStreamingResponder responder;
  rewriter_remote->Rewrite(kInputString, "unsafe", responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
}

TEST_F(AIRewriterTest, TextSafetySharedContext) {
  SetSolutionConfig([&]() {
    auto solution_config = CreateSolution();
    solution_config.mutable_feature()->set_can_skip_text_safety(false);
    return solution_config;
  }());

  const auto options = blink::mojom::AIRewriterCreateOptions::New(
      "unsafe", blink::mojom::AIRewriterTone::kAsIs,
      blink::mojom::AIRewriterFormat::kAsIs,
      blink::mojom::AIRewriterLength::kAsIs,
      /*expected_input_languages=*/std::vector<AILanguageCodePtr>(),
      /*expected_context_languages=*/std::vector<AILanguageCodePtr>(),
      /*output_language=*/AILanguageCode::New(""));

  mojo::Remote<blink::mojom::AIRewriter> rewriter_remote =
      GetAIRewriterRemote(options.Clone());
  AITestUtils::TestStreamingResponder responder;
  rewriter_remote->Rewrite(kInputString, kContextString,
                           responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
}

TEST_F(AIRewriterTest, TextSafetyOutput) {
  SetSolutionConfig([&]() {
    auto solution_config = CreateSolution();
    solution_config.mutable_feature()->set_can_skip_text_safety(false);
    solution_config.mutable_safety()
        ->mutable_partial_output_checks()
        ->set_minimum_tokens(1000);
    return solution_config;
  }());

  // Fake text safety checker looks for the string "unsafe".
  fake_broker_->settings().set_execute_result(
      {"a", "b", "c", "d", "e", "f", "g", "unsafe", "h"});
  auto rewriter_remote = GetAIRewriterRemote();
  AITestUtils::TestStreamingResponder responder;
  rewriter_remote->Rewrite(kInputString, kContextString,
                           responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
  EXPECT_TRUE(responder.responses().empty());
}

TEST_F(AIRewriterTest, TextSafetyOutputPartial) {
  SetSolutionConfig([&]() {
    auto solution_config = CreateSolution();
    solution_config.mutable_feature()->set_can_skip_text_safety(false);
    solution_config.mutable_safety()
        ->mutable_partial_output_checks()
        ->set_minimum_tokens(3);
    solution_config.mutable_safety()
        ->mutable_partial_output_checks()
        ->set_token_interval(2);
    return solution_config;
  }());

  // Fake text safety checker looks for the string "unsafe".
  fake_broker_->settings().set_execute_result(
      {"a", "b", "c", "d", "e", "f", "g", "unsafe", "h"});
  auto rewriter_remote = GetAIRewriterRemote();
  AITestUtils::TestStreamingResponder responder;
  rewriter_remote->Rewrite(kInputString, kContextString,
                           responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
  // Partial checks should still allow some output to stream.
  EXPECT_THAT(responder.responses(), ElementsAre("abc", "de", "fg"));
}

TEST_F(AIRewriterTest, ServiceCrash) {
  fake_broker_->settings().set_execute_result({"hi"});

  auto rewriter_remote = GetAIRewriterRemote();
  AITestUtils::TestStreamingResponder responder;
  rewriter_remote->Rewrite(kInputString, kContextString,
                           responder.BindRemote());
  fake_broker_->launcher().CrashService();

  EXPECT_FALSE(responder.WaitForCompletion());
  // TODO(crbug.com/494980521): Crashes should be yield kErrorSessionDestroyed.
  EXPECT_EQ(
      responder.error_status(),
      blink::mojom::ModelStreamingResponseStatus::kErrorFailedToCountTokens);

  rewriter_remote = GetAIRewriterRemote();
  EXPECT_THAT(Rewrite(*rewriter_remote, kInputString, kContextString),
              ElementsAre("hi"));
}

TEST_F(AIRewriterTest, CrashRecoveryMeasureInputUsage) {
  auto rewriter_remote = GetAIRewriterRemote();
  fake_broker_->launcher().CrashService();

  base::test::TestFuture<std::optional<uint32_t>> measure_future;
  rewriter_remote->MeasureUsage(kInputString, kContextString,
                                measure_future.GetCallback());

  auto size = std::string(kSharedContextString).size() +
              std::string(kContextString).size() +
              std::string(kInputString).size();
  EXPECT_EQ(measure_future.Get(), size);
}

TEST_F(AIRewriterTest, CanCreatePermissionsPolicyDisabled) {
  DisablePolicy(network::mojom::PermissionsPolicyFeature::kRewriter);
  mojo::test::BadMessageObserver observer;
  GetAIManagerRemote()->CanCreateRewriter(GetDefaultOptions(),
                                          base::DoNothing());
  EXPECT_EQ(observer.WaitForBadMessage(), "Permissions policy disabled");
}

TEST_F(AIRewriterTest, CreatePermissionsPolicyDisabled) {
  DisablePolicy(network::mojom::PermissionsPolicyFeature::kRewriter);
  mojo::test::BadMessageObserver observer;
  TestCreateRewriterClient create_rewriter_client;
  GetAIManagerRemote()->CreateRewriter(
      create_rewriter_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());
  EXPECT_EQ(observer.WaitForBadMessage(), "Policy or user setting disabled");
}

TEST_F(AIRewriterTest, CreateBuiltInAIAPIsEnterprisePolicyDisabled) {
  SetBuiltInAIAPIsEnterprisePolicy(false);
  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  GetAIManagerInterface()->CanCreateRewriter(GetDefaultOptions(),
                                             future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                              kUnavailableEnterprisePolicyDisabled);

  mojo::test::BadMessageObserver observer;
  TestCreateRewriterClient create_rewriter_client;
  GetAIManagerRemote()->CreateRewriter(
      create_rewriter_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());
  EXPECT_EQ(observer.WaitForBadMessage(), "Policy or user setting disabled");
  SetBuiltInAIAPIsEnterprisePolicy(true);
}

TEST_F(AIRewriterTest, CreateGenAILocalEnterprisePolicyDisabled) {
  SetGenAILocalEnterprisePolicy(false);
  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  GetAIManagerInterface()->CanCreateRewriter(GetDefaultOptions(),
                                             future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                              kUnavailableEnterprisePolicyDisabled);

  mojo::test::BadMessageObserver observer;
  TestCreateRewriterClient create_rewriter_client;
  GetAIManagerRemote()->CreateRewriter(
      create_rewriter_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());
  EXPECT_EQ(observer.WaitForBadMessage(), "Policy or user setting disabled");
  SetGenAILocalEnterprisePolicy(true);
}

TEST_F(AIRewriterTest, CreateOnDeviceAiUserSettingDisabled) {
  SetOnDeviceAiUserSetting(false);
  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  GetAIManagerInterface()->CanCreateRewriter(GetDefaultOptions(),
                                             future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                              kUnavailableFeatureNotEnabled);

  mojo::test::BadMessageObserver observer;
  TestCreateRewriterClient create_rewriter_client;
  GetAIManagerRemote()->CreateRewriter(
      create_rewriter_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());
  EXPECT_EQ(observer.WaitForBadMessage(), "Policy or user setting disabled");
  SetOnDeviceAiUserSetting(true);
}

#if !BUILDFLAG(IS_ANDROID)
class AIRewriterWithFeatureConfigTest : public AIRewriterTest {
 public:
  void SetupBroker() override {
    proto::WritingAssistanceApiFeatureConfig writer_cfg;
    writer_cfg.set_default_use_case("writing_assistance_api");
    (*writer_cfg.mutable_experimental_use_cases())["v4"] =
        "rewriter_gemma4";

    // Explicit BaseModelRecipeArgs and empty FakeBaseModelAsset::Content are
    // needed: ScenarioBuilder::AddBaseModel(name) defaults to 100 max_tokens
    // and non-empty cache weights (1015, 1016, 1017), which causes
    // FakeOnDeviceModel to emit dummy cache weight response chunks.
    constexpr uint32_t kDefaultMaxTokens = 8096;
    proto::SolutionConfig default_solution = CreateSolution();

    fake_broker_ = std::make_unique<optimization_guide::FakeManifestBroker>();
    optimization_guide::ScenarioBuilder(fake_broker_->component_state())
        .AddBaseModel(
            "base",
            optimization_guide::BaseModelRecipeArgs(
                proto::BaseModelRecipe::BACKEND_TYPE_GPU,
                proto::BaseModelRecipe::PERFORMANCE_HINT_HIGHEST_QUALITY,
                {}, kDefaultMaxTokens),
            optimization_guide::FakeBaseModelAsset::Content{}, "1.0.0.0")
        .AddBaseModel(
            "gemma4_base",
            optimization_guide::BaseModelRecipeArgs(
                proto::BaseModelRecipe::BACKEND_TYPE_GPU,
                proto::BaseModelRecipe::PERFORMANCE_HINT_HIGHEST_QUALITY,
                {}, kDefaultMaxTokens),
            optimization_guide::FakeBaseModelAsset::Content{}, "1.0.0.0")
        .AddSafetyModel("safety")
        .AddSafeSolution("writing_assistance_api", "base", "safety",
                         default_solution)
        .AddSafeSolution("rewriter_gemma4", "gemma4_base", "safety",
                         default_solution)
        .SetFeatureConfig("writing_assistance_api",
                          optimization_guide::AnyWrapProto(writer_cfg))
        .Finish();

    fake_broker_->settings().performance_class =
        on_device_model::mojom::PerformanceClass::kHigh;
    fake_broker_->Startup();
  }
};

TEST_F(AIRewriterWithFeatureConfigTest, CanCreateAndCreateWithManifestGemma4) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kAIApiFoundationalModel, {{"model_version", "v4"}});

  fake_broker_->client().RequestAssetsFor("rewriter_gemma4");
  ASSERT_TRUE(base::test::RunUntil([&] {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    ai_manager_->CanCreateRewriter(GetDefaultOptions(), future.GetCallback());
    return future.Get() ==
           blink::mojom::ModelAvailabilityCheckResult::kAvailable;
  }));

  // Verify CreateRewriter can retrieve the model successfully.
  TestCreateRewriterClient create_rewriter_client;
  GetAIManagerRemote()->CreateRewriter(
      create_rewriter_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());

  auto result = create_rewriter_client.result().Take();
  EXPECT_TRUE(result.has_value());
}

TEST_F(AIRewriterWithFeatureConfigTest, CanCreateBeforeDownloadGemma4) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kAIApiFoundationalModel, {{"model_version", "v4"}});

  // Assets are requested for writing_assistance_api, but since gemma4 is the
  // configured model_version, we should get kDownloadable for gemma4.
  fake_broker_->client().RequestAssetsFor("writing_assistance_api");

  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  ai_manager_->CanCreateRewriter(GetDefaultOptions(), future.GetCallback());
  EXPECT_EQ(future.Get(),
            blink::mojom::ModelAvailabilityCheckResult::kDownloadable);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

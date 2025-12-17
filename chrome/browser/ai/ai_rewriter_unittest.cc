// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_rewriter.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "chrome/browser/ai/features.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "components/optimization_guide/core/model_execution/test/mock_on_device_capability.h"
#include "components/optimization_guide/core/model_execution/test/substitution_builder.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/writing_assistance_api.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "content/public/browser/render_widget_host_view.h"
#include "services/on_device_model/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace {

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

  void OnResult(
      mojo::PendingRemote<::blink::mojom::AIRewriter> rewriter) override {
    result_.SetValue(std::move(rewriter));
  }

  void OnError(blink::mojom::AIManagerCreateClientError error,
               blink::mojom::QuotaErrorInfoPtr quota_error_info) override {
    result_.SetValue(
        base::unexpected(Error{error, std::move(quota_error_info)}));
  }

  TestFuture<CreateRewriterResult>& result() { return result_; }

 private:
  TestFuture<CreateRewriterResult> result_;
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

class AIRewriterTest : public AITestUtils::AITestBase {
 protected:
  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig CreateConfig()
      override {
    optimization_guide::proto::OnDeviceModelExecutionFeatureConfig config;
    config.set_can_skip_text_safety(true);
    config.set_feature(optimization_guide::proto::ModelExecutionFeature::
                           MODEL_EXECUTION_FEATURE_WRITING_ASSISTANCE_API);

    auto& input_config = *config.mutable_input_config();
    input_config.set_request_base_name(
        WritingAssistanceApiRequest().GetTypeName());

    *input_config.add_execute_substitutions() = FieldSubstitution(
        "%s", ProtoField({WritingAssistanceApiRequest::kContextFieldNumber}));
    *input_config.add_execute_substitutions() = FieldSubstitution(
        "%s",
        ProtoField({WritingAssistanceApiRequest::kSharedContextFieldNumber}));
    *input_config.add_execute_substitutions() = FieldSubstitution(
        "%s",
        ProtoField({WritingAssistanceApiRequest::kRewriteTextFieldNumber}));

    auto& output_config = *config.mutable_output_config();
    output_config.set_proto_type(WritingAssistanceApiResponse().GetTypeName());
    *output_config.mutable_proto_field() = StringValueField();

    return config;
  }

  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig
  CreateSafeConfig() {
    auto config = CreateConfig();
    config.set_can_skip_text_safety(false);
    return config;
  }

  mojo::Remote<blink::mojom::AIRewriter> GetAIRewriterRemote(
      blink::mojom::AIRewriterCreateOptionsPtr options = GetDefaultOptions()) {
    TestCreateRewriterClient create_rewriter_client;
    GetAIManagerRemote()->CreateRewriter(
        create_rewriter_client.BindNewPipeAndPassRemote(), std::move(options));

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
};

TEST_F(AIRewriterTest, CreateRewriterNoService) {
  SetupNullOptimizationGuideKeyedService();

  TestCreateRewriterClient create_rewriter_client;
  GetAIManagerRemote()->CreateRewriter(
      create_rewriter_client.BindNewPipeAndPassRemote(), GetDefaultOptions());

  CreateRewriterResult result = create_rewriter_client.result().Take();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

TEST_F(AIRewriterTest, CanCreateWaitsForEligibility) {
  base::test::TestFuture<base::OnceCallback<void(
      optimization_guide::OnDeviceModelEligibilityReason)>>
      eligibility_future;

  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              GetOnDeviceModelEligibilityAsync(_, _, _))
      .WillOnce([&](auto feature, auto capabilities, auto callback) {
        eligibility_future.SetValue(std::move(callback));
      });

  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult>
      result_future;
  GetAIManagerInterface()->CanCreateRewriter(GetDefaultOptions(),
                                             result_future.GetCallback());
  // Session should not be ready until eligibility callback has run.
  EXPECT_FALSE(result_future.IsReady());
  eligibility_future.Take().Run(
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);
  EXPECT_EQ(result_future.Get(),
            blink::mojom::ModelAvailabilityCheckResult::kAvailable);
}

TEST_F(AIRewriterTest, CanCreateUnavailableWhenAdaptationNotAvailable) {
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              GetOnDeviceModelEligibilityAsync(_, _, _))
      .WillOnce([&](auto feature, auto capabilities, auto callback) {
        std::move(callback).Run(
            optimization_guide::OnDeviceModelEligibilityReason::
                kModelAdaptationNotAvailable);
      });

  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult>
      result_future;
  GetAIManagerInterface()->CanCreateRewriter(GetDefaultOptions(),
                                             result_future.GetCallback());
  EXPECT_EQ(result_future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                                     kUnavailableModelAdaptationNotAvailable);
}

TEST_F(AIRewriterTest, CreateRewriterModelNotEligible) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{optimization_guide::features::kOnDeviceModelPerformanceParams,
        {{"compatible_on_device_performance_classes", "3,4,5,6"}}}},
      {{on_device_model::features::kOnDeviceModelCpuBackend}});

  fake_broker_->service_settings().performance_class =
      PerformanceClass::kVeryLow;

  TestCreateRewriterClient create_rewriter_client;
  GetAIManagerRemote()->CreateRewriter(
      create_rewriter_client.BindNewPipeAndPassRemote(), GetDefaultOptions());

  CreateRewriterResult result = create_rewriter_client.result().Take();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

TEST_F(AIRewriterTest, CreateRewriterWaitsForBaseModel) {
  fake_broker_->InstallBaseModel(nullptr);

  TestCreateRewriterClient create_rewriter_client;
  GetAIManagerRemote()->CreateRewriter(
      create_rewriter_client.BindNewPipeAndPassRemote(), GetDefaultOptions());

  TestFuture<CreateRewriterResult>& future = create_rewriter_client.result();
  task_environment()->FastForwardBy(base::Hours(1));
  EXPECT_FALSE(future.IsReady());

  fake_broker_->InstallBaseModel(
      std::make_unique<optimization_guide::FakeBaseModelAsset>());

  EXPECT_OK(future.Take());
}

TEST_F(AIRewriterTest, CreateRewriterWaitsForModelAdaptation) {
  fake_broker_->model_provider().RemoveModel(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_WRITING_ASSISTANCE_API);

  TestCreateRewriterClient create_rewriter_client;
  GetAIManagerRemote()->CreateRewriter(
      create_rewriter_client.BindNewPipeAndPassRemote(), GetDefaultOptions());

  TestFuture<CreateRewriterResult>& future = create_rewriter_client.result();
  task_environment()->FastForwardBy(base::Hours(1));
  EXPECT_FALSE(future.IsReady());

  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);

  EXPECT_OK(future.Take());
}

TEST_F(AIRewriterTest, CreateRewriterWaitsForTextSafetyModel) {
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);

  TestCreateRewriterClient create_rewriter_client;
  GetAIManagerRemote()->CreateRewriter(
      create_rewriter_client.BindNewPipeAndPassRemote(), GetDefaultOptions());

  TestFuture<CreateRewriterResult>& future = create_rewriter_client.result();
  task_environment()->FastForwardBy(base::Hours(1));
  EXPECT_FALSE(future.IsReady());

  optimization_guide::FakeSafetyModelAsset safety_asset(CreateSafetyConfig());
  fake_broker_->UpdateSafetyModel(safety_asset);

  EXPECT_OK(future.Take());
}

TEST_F(AIRewriterTest, CreateRewriterSafetyConfigNotAvailable) {
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);
  // Provide a safety asset that does not support rewriter.
  optimization_guide::FakeSafetyModelAsset safety_asset([] {
    auto safety_config = CreateSafetyConfig();
    safety_config.set_feature(
        optimization_guide::proto::MODEL_EXECUTION_FEATURE_TEST);
    return safety_config;
  }());
  fake_broker_->UpdateSafetyModel(safety_asset);

  TestCreateRewriterClient create_rewriter_client;
  GetAIManagerRemote()->CreateRewriter(
      create_rewriter_client.BindNewPipeAndPassRemote(), GetDefaultOptions());

  CreateRewriterResult result = create_rewriter_client.result().Take();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

TEST_F(AIRewriterTest, CreateRewriterUnableToCalculateTokenSize) {
  // Incorrect `request_base_name` cause session to fail constructing input
  // string and checking token size.
  auto config = CreateConfig();
  auto& input_config = *config.mutable_input_config();
  input_config.set_request_base_name("InvalidRequestBaseName");

  optimization_guide::FakeAdaptationAsset fake_asset({.config = config});
  fake_broker_->UpdateModelAdaptation(fake_asset);

  TestCreateRewriterClient create_rewriter_client;
  GetAIManagerRemote()->CreateRewriter(
      create_rewriter_client.BindNewPipeAndPassRemote(), GetDefaultOptions());

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
      create_rewriter_client.BindNewPipeAndPassRemote(), GetDefaultOptions());

  CreateRewriterResult result = create_rewriter_client.result().Take();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kInitialInputTooLarge);
  EXPECT_EQ(result.error().quota_error_info->requested,
            blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);
  EXPECT_EQ(result.error().quota_error_info->quota,
            blink::mojom::kWritingAssistanceMaxInputTokenSize);
}

TEST_F(AIRewriterTest, CanCreateDefaultOptions) {
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              GetOnDeviceModelEligibilityAsync(_, _, _))
      .WillOnce([](auto feature, auto capabilities, auto callback) {
        std::move(callback).Run(
            optimization_guide::OnDeviceModelEligibilityReason::kSuccess);
      });
  base::MockCallback<AIManager::CanCreateRewriterCallback> callback;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::kAvailable));
  GetAIManagerInterface()->CanCreateRewriter(GetDefaultOptions(),
                                             callback.Get());
}

TEST_F(AIRewriterTest, CanCreateIsLanguagesSupported) {
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              GetOnDeviceModelEligibilityAsync(_, _, _))
      .WillOnce([](auto feature, auto capabilities, auto callback) {
        std::move(callback).Run(
            optimization_guide::OnDeviceModelEligibilityReason::kSuccess);
      });
  auto options = GetDefaultOptions();
  options->output_language = AILanguageCode::New("en");
  options->expected_input_languages =
      AITestUtils::ToMojoLanguageCodes({"en-US", ""});
  options->expected_context_languages =
      AITestUtils::ToMojoLanguageCodes({"en-GB", ""});
  base::MockCallback<AIManager::CanCreateRewriterCallback> callback;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::kAvailable));
  GetAIManagerInterface()->CanCreateRewriter(std::move(options),
                                             callback.Get());
}

TEST_F(AIRewriterTest, CanCreateUnIsLanguagesSupported) {
  auto options = GetDefaultOptions();
  options->output_language = AILanguageCode::New("es-ES");
  options->expected_input_languages =
      AITestUtils::ToMojoLanguageCodes({"en", "fr", "ja"});
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
      {"en", "English"},  {"en-us", "English"},  {"en-uk", "English"},
      {"es", "Spanish"},  {"es-sp", "Spanish"},  {"es-mx", "Spanish"},
      {"ja", "Japanese"}, {"ja-jp", "Japanese"}, {"ja-foo", "Japanese"},
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

  main_rfh()->GetRenderWidgetHost()->GetView()->Hide();
  EXPECT_THAT(Rewrite(*rewriter_remote, kInputString, kContextString),
              ElementsAre("Priority: background", "hi"));

  main_rfh()->GetRenderWidgetHost()->GetView()->Show();
  EXPECT_THAT(Rewrite(*rewriter_remote, kInputString, kContextString),
              ElementsAre("hi"));
}

TEST_F(AIRewriterTest, TextSafetyInput) {
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);
  optimization_guide::FakeSafetyModelAsset safety_asset(CreateSafetyConfig());
  fake_broker_->UpdateSafetyModel(safety_asset);

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
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);
  optimization_guide::FakeSafetyModelAsset safety_asset(CreateSafetyConfig());
  fake_broker_->UpdateSafetyModel(safety_asset);

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
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);
  optimization_guide::FakeSafetyModelAsset safety_asset(CreateSafetyConfig());
  fake_broker_->UpdateSafetyModel(safety_asset);

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
  fake_broker_->CrashService();

  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure);

  rewriter_remote = GetAIRewriterRemote();
  EXPECT_THAT(Rewrite(*rewriter_remote, kInputString, kContextString),
              ElementsAre("hi"));
}

TEST_F(AIRewriterTest, CrashRecoveryMeasureInputUsage) {
  auto rewriter_remote = GetAIRewriterRemote();
  fake_broker_->CrashService();

  base::test::TestFuture<std::optional<uint32_t>> measure_future;
  rewriter_remote->MeasureUsage(kInputString, kContextString,
                                measure_future.GetCallback());

  auto size = std::string(kSharedContextString).size() +
              std::string(kContextString).size() +
              std::string(kInputString).size();
  EXPECT_EQ(measure_future.Get(), size);
}

}  // namespace

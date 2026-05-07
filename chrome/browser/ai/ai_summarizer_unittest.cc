// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_summarizer.h"

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
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/fake_manifest_broker.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/scenario_builder.h"
#include "components/optimization_guide/core/model_execution/test/mock_on_device_capability.h"
#include "components/optimization_guide/core/model_execution/test/substitution_builder.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/feature_configs.pb.h"
#include "components/optimization_guide/proto/features/summarize.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "content/public/browser/render_widget_host_view.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "services/on_device_model/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
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
using ::optimization_guide::proto::SummarizeRequest;
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

using CreateSummarizerResult =
    base::expected<mojo::PendingRemote<blink::mojom::AISummarizer>, Error>;

class TestCreateSummarizerClient
    : public blink::mojom::AIManagerCreateSummarizerClient {
 public:
  TestCreateSummarizerClient() = default;
  ~TestCreateSummarizerClient() override = default;
  TestCreateSummarizerClient(const TestCreateSummarizerClient&) = delete;
  TestCreateSummarizerClient& operator=(const TestCreateSummarizerClient&) =
      delete;

  mojo::PendingRemote<blink::mojom::AIManagerCreateSummarizerClient>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnResult(
      mojo::PendingRemote<::blink::mojom::AISummarizer> summarizer) override {
    result_.SetValue(std::move(summarizer));
  }

  void OnError(blink::mojom::AIManagerCreateClientError error,
               blink::mojom::QuotaErrorInfoPtr quota_error_info) override {
    result_.SetValue(
        base::unexpected(Error{error, std::move(quota_error_info)}));
  }

  TestFuture<CreateSummarizerResult>& result() { return result_; }

 private:
  TestFuture<CreateSummarizerResult> result_;
  mojo::Receiver<blink::mojom::AIManagerCreateSummarizerClient> receiver_{this};
};

blink::mojom::AISummarizerCreateOptionsPtr GetDefaultOptions() {
  return blink::mojom::AISummarizerCreateOptions::New(
      /*shared_context=*/"", blink::mojom::AISummarizerType::kKeyPoints,
      blink::mojom::AISummarizerFormat::kMarkDown,
      blink::mojom::AISummarizerLength::kShort,
      blink::mojom::PerformancePreference::kAuto,
      /*expected_input_languages=*/std::vector<AILanguageCodePtr>(),
      /*expected_context_languages=*/std::vector<AILanguageCodePtr>(),
      /*output_language=*/AILanguageCode::New(""));
}

optimization_guide::proto::FeatureTextSafetyConfiguration CreateSafetyConfig() {
  optimization_guide::proto::FeatureTextSafetyConfiguration safety_config;
  safety_config.set_feature(
      optimization_guide::proto::MODEL_EXECUTION_FEATURE_SUMMARIZE);
  safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
  {
    auto* check = safety_config.add_request_check();
    check->mutable_input_template()->Add(FieldSubstitution(
        "%s", ProtoField({SummarizeRequest::kArticleFieldNumber})));
  }
  {
    auto* check = safety_config.add_request_check();
    check->mutable_input_template()->Add(FieldSubstitution(
        "%s", ProtoField({SummarizeRequest::kContextFieldNumber})));
  }
  return safety_config;
}

class AISummarizerTest : public AITestUtils::AITestBase {
 protected:
  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig CreateConfig()
      override {
    optimization_guide::proto::OnDeviceModelExecutionFeatureConfig config;
    config.set_can_skip_text_safety(true);
    config.set_feature(optimization_guide::proto::ModelExecutionFeature::
                           MODEL_EXECUTION_FEATURE_SUMMARIZE);

    auto& input_config = *config.mutable_input_config();
    input_config.set_request_base_name(SummarizeRequest().GetTypeName());

    *input_config.add_execute_substitutions() = FieldSubstitution(
        "%s", ProtoField({SummarizeRequest::kArticleFieldNumber}));
    *input_config.add_execute_substitutions() = FieldSubstitution(
        "%s", ProtoField({SummarizeRequest::kContextFieldNumber}));

    auto& output_config = *config.mutable_output_config();
    output_config.set_proto_type(
        optimization_guide::proto::StringValue().GetTypeName());
    *output_config.mutable_proto_field() = StringValueField();

    return config;
  }

  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig
  CreateSafeConfig() {
    auto config = CreateConfig();
    config.set_can_skip_text_safety(false);
    return config;
  }

  mojo::Remote<blink::mojom::AISummarizer> GetAISummarizerRemote(
      blink::mojom::AISummarizerCreateOptionsPtr options =
          GetDefaultOptions()) {
    TestCreateSummarizerClient create_summarizer_client;
    GetAIManagerRemote()->CreateSummarizer(
        create_summarizer_client.BindNewPipeAndPassRemote(),
        std::move(options));

    CreateSummarizerResult result = create_summarizer_client.result().Take();
    EXPECT_OK(result);
    return mojo::Remote<blink::mojom::AISummarizer>(std::move(result.value()));
  }

  void RunSimpleSummarizeTest(blink::mojom::AISummarizerType type,
                              blink::mojom::AISummarizerFormat format,
                              blink::mojom::AISummarizerLength length) {
    fake_broker_->settings().set_execute_result({"Result text"});

    const auto options = blink::mojom::AISummarizerCreateOptions::New(
        /*shared_context=*/"", type, format, length,
        blink::mojom::PerformancePreference::kAuto,
        /*expected_input_languages=*/std::vector<AILanguageCodePtr>(),
        /*expected_context_languages=*/std::vector<AILanguageCodePtr>(),
        /*output_language=*/AILanguageCode::New(""));
    mojo::Remote<blink::mojom::AISummarizer> summarizer_remote =
        GetAISummarizerRemote(options.Clone());

    EXPECT_THAT(Summarize(*summarizer_remote, kInputString, kContextString),
                ElementsAreArray({"Result text"}));
  }

  std::vector<std::string> Summarize(blink::mojom::AISummarizer& summarizer,
                                     const std::string& input,
                                     const std::string& context) {
    AITestUtils::TestStreamingResponder responder;
    summarizer.Summarize(input, context, responder.BindRemote());
    EXPECT_TRUE(responder.WaitForCompletion());
    // Return Summarizer's response without the final empty string chunk.
    return responder.responses_without_last();
  }

  void EnsureModelIsReady() {
    TestCreateSummarizerClient summarizer_client;
    GetAIManagerRemote()->CreateSummarizer(
        summarizer_client.BindNewPipeAndPassRemote(), GetDefaultOptions());

    auto result = summarizer_client.result().Take();
    EXPECT_OK(result);
  }
};

TEST(AISummarizerStandaloneTest, CombineContexts) {
  EXPECT_EQ("", AISummarizer::CombineContexts("", ""));
  EXPECT_EQ("a\n", AISummarizer::CombineContexts("a", ""));
  EXPECT_EQ("b\n", AISummarizer::CombineContexts("", "b"));
  EXPECT_EQ("a b\n", AISummarizer::CombineContexts("a", "b"));
}

TEST_F(AISummarizerTest, CanCreateDefaultOptions) {
  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    GetAIManagerInterface()->CanCreateSummarizer(GetDefaultOptions(),
                                                 future.GetCallback());
    EXPECT_EQ(future.Get(),
              blink::mojom::ModelAvailabilityCheckResult::kDownloadable);
  }

  // After model is ready, `CanCreateSummarizer` should return available.
  EnsureModelIsReady();

  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    GetAIManagerInterface()->CanCreateSummarizer(GetDefaultOptions(),
                                                 future.GetCallback());
    EXPECT_EQ(future.Get(),
              blink::mojom::ModelAvailabilityCheckResult::kAvailable);
  }
}

TEST_F(AISummarizerTest, CanCreateIsLanguagesSupported) {
  EnsureModelIsReady();

  auto options = GetDefaultOptions();
  options->output_language = AILanguageCode::New("en");
  options->expected_input_languages =
      AITestUtils::ToMojoLanguageCodes({"en-US", ""});
  options->expected_context_languages =
      AITestUtils::ToMojoLanguageCodes({"en-GB", ""});

  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  GetAIManagerInterface()->CanCreateSummarizer(std::move(options),
                                               future.GetCallback());
  EXPECT_EQ(future.Get(),
            blink::mojom::ModelAvailabilityCheckResult::kAvailable);
}

TEST_F(AISummarizerTest, CanCreateUnIsLanguagesSupported) {
  auto options = GetDefaultOptions();
  options->output_language = AILanguageCode::New("es-ES");
  options->expected_input_languages =
      AITestUtils::ToMojoLanguageCodes({"en", "tlh", "ja"});
  options->expected_context_languages =
      AITestUtils::ToMojoLanguageCodes({"ar", "zh", "hi"});
  base::MockCallback<AIManager::CanCreateSummarizerCallback> callback;
  EXPECT_CALL(callback, Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage));
  GetAIManagerInterface()->CanCreateSummarizer(std::move(options),
                                               callback.Get());
}

TEST_F(AISummarizerTest, ToProtoOptionsLanguagesSupported) {
  // Summarizer proto expects a limited set of BCP 47 base language codes.
  std::vector<std::pair<std::string, std::string>> languages = {
      {"en", "en"}, {"en-us", "en"}, {"en-uk", "en"},
      {"es", "es"}, {"es-sp", "es"}, {"es-mx", "es"},
      {"ja", "ja"}, {"ja-jp", "ja"}, {"ja-foo", "ja"},
  };
  blink::mojom::AISummarizerCreateOptionsPtr options = GetDefaultOptions();
  for (const auto& language : languages) {
    options->output_language = AILanguageCode::New(language.first);
    const auto proto_options = AISummarizer::ToProtoOptions(options);
    EXPECT_EQ(proto_options->output_language(), language.second);
  }
}

TEST_F(AISummarizerTest, CreateSummarizerNoService) {
  SetupNullOptimizationGuideKeyedService();

  TestCreateSummarizerClient create_summarizer_client;
  GetAIManagerRemote()->CreateSummarizer(
      create_summarizer_client.BindNewPipeAndPassRemote(), GetDefaultOptions());

  CreateSummarizerResult result = create_summarizer_client.result().Take();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

TEST_F(AISummarizerTest, CreateSummarizerModelNotEligible) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{optimization_guide::features::kOnDeviceModelPerformanceParams,
        {{"compatible_on_device_performance_classes", "3,4,5,6"}}}},
      {{on_device_model::features::kOnDeviceModelCpuBackend}});

  fake_broker_->service_settings().performance_class =
      PerformanceClass::kVeryLow;

  TestCreateSummarizerClient create_summarizer_client;
  GetAIManagerRemote()->CreateSummarizer(
      create_summarizer_client.BindNewPipeAndPassRemote(), GetDefaultOptions());

  CreateSummarizerResult result = create_summarizer_client.result().Take();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

TEST_F(AISummarizerTest, CreateSummarizerWaitsForBaseModel) {
  fake_broker_->InstallBaseModel(nullptr);

  TestCreateSummarizerClient create_summarizer_client;
  GetAIManagerRemote()->CreateSummarizer(
      create_summarizer_client.BindNewPipeAndPassRemote(), GetDefaultOptions());

  TestFuture<CreateSummarizerResult>& future =
      create_summarizer_client.result();
  task_environment()->FastForwardBy(base::Hours(1));
  EXPECT_FALSE(future.IsReady());

  fake_broker_->InstallBaseModel(
      std::make_unique<optimization_guide::FakeBaseModelAsset>());

  EXPECT_OK(future.Take());
}

TEST_F(AISummarizerTest, CreateSummarizerWaitsForModelAdaptation) {
  fake_broker_->model_provider().RemoveModel(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_SUMMARIZE);

  TestCreateSummarizerClient create_summarizer_client;
  GetAIManagerRemote()->CreateSummarizer(
      create_summarizer_client.BindNewPipeAndPassRemote(), GetDefaultOptions());

  TestFuture<CreateSummarizerResult>& future =
      create_summarizer_client.result();
  task_environment()->FastForwardBy(base::Hours(1));
  EXPECT_FALSE(future.IsReady());

  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);

  EXPECT_OK(future.Take());
}

TEST_F(AISummarizerTest, CreateSummarizerWaitsForTextSafetyModel) {
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);

  TestCreateSummarizerClient create_summarizer_client;
  GetAIManagerRemote()->CreateSummarizer(
      create_summarizer_client.BindNewPipeAndPassRemote(), GetDefaultOptions());

  TestFuture<CreateSummarizerResult>& future =
      create_summarizer_client.result();
  task_environment()->FastForwardBy(base::Hours(1));
  EXPECT_FALSE(future.IsReady());

  optimization_guide::FakeSafetyModelAsset safety_asset(CreateSafetyConfig());
  fake_broker_->UpdateSafetyModel(safety_asset);

  EXPECT_OK(future.Take());
}

TEST_F(AISummarizerTest, CreateSummarizerSafetyConfigNotAvailable) {
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);
  // Provide a safety asset that does not support summarizer.
  optimization_guide::FakeSafetyModelAsset safety_asset([] {
    auto safety_config = CreateSafetyConfig();
    safety_config.set_feature(
        optimization_guide::proto::MODEL_EXECUTION_FEATURE_TEST);
    return safety_config;
  }());
  fake_broker_->UpdateSafetyModel(safety_asset);

  TestCreateSummarizerClient create_summarizer_client;
  GetAIManagerRemote()->CreateSummarizer(
      create_summarizer_client.BindNewPipeAndPassRemote(), GetDefaultOptions());

  CreateSummarizerResult result = create_summarizer_client.result().Take();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

TEST_F(AISummarizerTest, CreateSummarizerUnableToCalculateTokenSize) {
  // Incorrect `request_base_name` cause session to fail constructing input
  // string and checking token size.
  auto config = CreateConfig();
  auto& input_config = *config.mutable_input_config();
  input_config.set_request_base_name("InvalidRequestBaseName");

  optimization_guide::FakeAdaptationAsset fake_asset({.config = config});
  fake_broker_->UpdateModelAdaptation(fake_asset);

  TestCreateSummarizerClient create_summarizer_client;
  auto options = GetDefaultOptions();
  options->shared_context = kSharedContextString;
  GetAIManagerRemote()->CreateSummarizer(
      create_summarizer_client.BindNewPipeAndPassRemote(), std::move(options));

  CreateSummarizerResult result = create_summarizer_client.result().Take();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(
      result.error().error,
      blink::mojom::AIManagerCreateClientError::kUnableToCalculateTokenSize);
}

TEST_F(AISummarizerTest, CreateSummarizerContextLimitExceededError) {
  fake_broker_->settings().set_size_in_tokens(
      blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);

  TestCreateSummarizerClient create_summarizer_client;
  auto options = GetDefaultOptions();
  options->shared_context = kSharedContextString;
  GetAIManagerRemote()->CreateSummarizer(
      create_summarizer_client.BindNewPipeAndPassRemote(), std::move(options));

  CreateSummarizerResult result = create_summarizer_client.result().Take();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kInitialInputTooLarge);
  EXPECT_EQ(result.error().quota_error_info->requested,
            blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);
  EXPECT_EQ(result.error().quota_error_info->quota,
            blink::mojom::kWritingAssistanceMaxInputTokenSize);
}

TEST_F(AISummarizerTest, SummarizeDefault) {
  RunSimpleSummarizeTest(blink::mojom::AISummarizerType::kTLDR,
                         blink::mojom::AISummarizerFormat::kPlainText,
                         blink::mojom::AISummarizerLength::kMedium);
}

TEST_F(AISummarizerTest, SummarizeWithOptions) {
  blink::mojom::AISummarizerType types[]{
      blink::mojom::AISummarizerType::kTLDR,
      blink::mojom::AISummarizerType::kKeyPoints,
      blink::mojom::AISummarizerType::kTeaser,
      blink::mojom::AISummarizerType::kHeadline,
  };
  blink::mojom::AISummarizerFormat formats[]{
      blink::mojom::AISummarizerFormat::kPlainText,
      blink::mojom::AISummarizerFormat::kMarkDown,
  };
  blink::mojom::AISummarizerLength lengths[]{
      blink::mojom::AISummarizerLength::kShort,
      blink::mojom::AISummarizerLength::kMedium,
      blink::mojom::AISummarizerLength::kLong,
  };
  for (const auto& type : types) {
    for (const auto& format : formats) {
      for (const auto& length : lengths) {
        SCOPED_TRACE(testing::Message()
                     << type << " " << format << " " << length);
        RunSimpleSummarizeTest(type, format, length);
      }
    }
  }
}

TEST_F(AISummarizerTest, InputLimitExceededError) {
  auto summarizer_remote = GetAISummarizerRemote();

  fake_broker_->settings().set_size_in_tokens(
      blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);

  AITestUtils::TestStreamingResponder responder;
  summarizer_remote->Summarize(kInputString, kContextString,
                               responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorInputTooLarge);
  ASSERT_EQ(responder.quota_error_info().requested,
            blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);
  ASSERT_EQ(responder.quota_error_info().quota,
            blink::mojom::kWritingAssistanceMaxInputTokenSize);
}

TEST_F(AISummarizerTest, SummarizeMultipleResponse) {
  auto summarizer_remote = GetAISummarizerRemote();

  std::vector<std::string> result = {"Result ", "text"};
  fake_broker_->settings().set_execute_result(result);
  EXPECT_THAT(Summarize(*summarizer_remote, kInputString, kContextString),
              ElementsAreArray(result));
}

TEST_F(AISummarizerTest, MultipleSummarize) {
  auto summarizer_remote = GetAISummarizerRemote();

  std::vector<std::string> result = {"Result ", "text"};
  fake_broker_->settings().set_execute_result(result);
  EXPECT_THAT(Summarize(*summarizer_remote, kInputString, kContextString),
              ElementsAreArray(result));

  std::vector<std::string> result2 = {"Result ", "text ", "2"};
  fake_broker_->settings().set_execute_result(result2);
  EXPECT_THAT(Summarize(*summarizer_remote, "input string 2", "test context 2"),
              ElementsAreArray(result2));
}

TEST_F(AISummarizerTest, MeasureUsage) {
  auto options = GetDefaultOptions();
  options->shared_context = kSharedContextString;
  auto summarizer_remote = GetAISummarizerRemote(std::move(options));

  base::test::TestFuture<std::optional<uint32_t>> measure_future;
  summarizer_remote->MeasureUsage(kInputString, kContextString,
                                  measure_future.GetCallback());

  std::string context =
      AISummarizer::CombineContexts(kSharedContextString, kContextString);
  EXPECT_EQ(measure_future.Get(),
            std::string(kInputString).size() + context.size());
}

TEST_F(AISummarizerTest, Priority) {
  fake_broker_->settings().set_execute_result({"hi"});
  auto summarizer_remote = GetAISummarizerRemote();

  EXPECT_THAT(Summarize(*summarizer_remote, kInputString, kContextString),
              ElementsAre("hi"));

  main_rfh()->GetRenderWidgetHost()->GetView()->Hide();
  EXPECT_THAT(Summarize(*summarizer_remote, kInputString, kContextString),
              ElementsAre("Priority: background", "hi"));

  main_rfh()->GetRenderWidgetHost()->GetView()->Show();
  EXPECT_THAT(Summarize(*summarizer_remote, kInputString, kContextString),
              ElementsAre("hi"));
}

TEST_F(AISummarizerTest, TextSafetyInput) {
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);
  optimization_guide::FakeSafetyModelAsset safety_asset(CreateSafetyConfig());
  fake_broker_->UpdateSafetyModel(safety_asset);

  fake_broker_->settings().set_execute_result({"hi"});
  auto summarizer_remote = GetAISummarizerRemote();
  EXPECT_THAT(Summarize(*summarizer_remote, kInputString, kContextString),
              ElementsAre("hi"));

  AITestUtils::TestStreamingResponder responder;
  summarizer_remote->Summarize("unsafe", kContextString,
                               responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
}

TEST_F(AISummarizerTest, TextSafetyContext) {
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);
  optimization_guide::FakeSafetyModelAsset safety_asset(CreateSafetyConfig());
  fake_broker_->UpdateSafetyModel(safety_asset);

  fake_broker_->settings().set_execute_result({"hi"});
  auto summarizer_remote = GetAISummarizerRemote();
  EXPECT_THAT(Summarize(*summarizer_remote, kInputString, kContextString),
              ElementsAre("hi"));

  AITestUtils::TestStreamingResponder responder;
  summarizer_remote->Summarize(kInputString, "unsafe", responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
}

TEST_F(AISummarizerTest, TextSafetySharedContext) {
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);
  optimization_guide::FakeSafetyModelAsset safety_asset(CreateSafetyConfig());
  fake_broker_->UpdateSafetyModel(safety_asset);

  const auto options = blink::mojom::AISummarizerCreateOptions::New(
      "unsafe", blink::mojom::AISummarizerType::kTLDR,
      blink::mojom::AISummarizerFormat::kPlainText,
      blink::mojom::AISummarizerLength::kMedium,
      blink::mojom::PerformancePreference::kAuto,
      /*expected_input_languages=*/std::vector<AILanguageCodePtr>(),
      /*expected_context_languages=*/std::vector<AILanguageCodePtr>(),
      /*output_language=*/AILanguageCode::New(""));

  mojo::Remote<blink::mojom::AISummarizer> summarizer_remote =
      GetAISummarizerRemote(options.Clone());
  AITestUtils::TestStreamingResponder responder;
  summarizer_remote->Summarize(kInputString, kContextString,
                               responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
}

TEST_F(AISummarizerTest, TextSafetyOutput) {
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
  auto summarizer_remote = GetAISummarizerRemote();
  AITestUtils::TestStreamingResponder responder;
  summarizer_remote->Summarize(kInputString, kContextString,
                               responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
  EXPECT_TRUE(responder.responses().empty());
}

TEST_F(AISummarizerTest, TextSafetyOutputPartial) {
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
  auto summarizer_remote = GetAISummarizerRemote();
  AITestUtils::TestStreamingResponder responder;
  summarizer_remote->Summarize(kInputString, kContextString,
                               responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
  // Partial checks should still allow some output to stream.
  EXPECT_THAT(responder.responses(), ElementsAre("abc", "de", "fg"));
}

TEST_F(AISummarizerTest, ServiceCrash) {
  fake_broker_->settings().set_execute_result({"hi"});

  auto summarizer_remote = GetAISummarizerRemote();
  AITestUtils::TestStreamingResponder responder;
  summarizer_remote->Summarize(kInputString, kContextString,
                               responder.BindRemote());
  fake_broker_->CrashService();

  EXPECT_FALSE(responder.WaitForCompletion());
  // TODO(crbug.com/494980521): Crashes should be yield kErrorSessionDestroyed.
  EXPECT_EQ(
      responder.error_status(),
      blink::mojom::ModelStreamingResponseStatus::kErrorFailedToCountTokens);

  summarizer_remote = GetAISummarizerRemote();
  EXPECT_THAT(Summarize(*summarizer_remote, kInputString, kContextString),
              ElementsAre("hi"));
}

TEST_F(AISummarizerTest, CrashRecoveryMeasureInputUsage) {
  auto options = GetDefaultOptions();
  options->shared_context = kSharedContextString;
  auto summarizer_remote = GetAISummarizerRemote(std::move(options));
  fake_broker_->CrashService();

  base::test::TestFuture<std::optional<uint32_t>> measure_future;
  summarizer_remote->MeasureUsage(kInputString, kContextString,
                                  measure_future.GetCallback());

  std::string context =
      AISummarizer::CombineContexts(kSharedContextString, kContextString);
  EXPECT_EQ(measure_future.Get(),
            std::string(kInputString).size() + context.size());
}

TEST_F(AISummarizerTest, CanCreatePermissionsPolicyDisabled) {
  DisablePolicy(network::mojom::PermissionsPolicyFeature::kSummarizer);
  mojo::test::BadMessageObserver observer;
  GetAIManagerRemote()->CanCreateSummarizer(GetDefaultOptions(),
                                            base::DoNothing());
  EXPECT_EQ(observer.WaitForBadMessage(), "Permissions policy disabled");
}

TEST_F(AISummarizerTest, CreatePermissionsPolicyDisabled) {
  DisablePolicy(network::mojom::PermissionsPolicyFeature::kSummarizer);
  mojo::test::BadMessageObserver observer;
  TestCreateSummarizerClient create_summarizer_client;
  GetAIManagerRemote()->CreateSummarizer(
      create_summarizer_client.BindNewPipeAndPassRemote(), GetDefaultOptions());
  EXPECT_EQ(observer.WaitForBadMessage(), "Policy or user setting disabled");
}

TEST_F(AISummarizerTest, CreateBuiltInAIAPIsEnterprisePolicyDisabled) {
  SetBuiltInAIAPIsEnterprisePolicy(false);
  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  GetAIManagerInterface()->CanCreateSummarizer(GetDefaultOptions(),
                                               future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                              kUnavailableEnterprisePolicyDisabled);

  mojo::test::BadMessageObserver observer;
  TestCreateSummarizerClient create_summarizer_client;
  GetAIManagerRemote()->CreateSummarizer(
      create_summarizer_client.BindNewPipeAndPassRemote(), GetDefaultOptions());
  EXPECT_EQ(observer.WaitForBadMessage(), "Policy or user setting disabled");
  SetBuiltInAIAPIsEnterprisePolicy(true);
}

TEST_F(AISummarizerTest, CreateGenAILocalEnterprisePolicyDisabled) {
  SetGenAILocalEnterprisePolicy(false);
  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  GetAIManagerInterface()->CanCreateSummarizer(GetDefaultOptions(),
                                               future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                              kUnavailableEnterprisePolicyDisabled);

  mojo::test::BadMessageObserver observer;
  TestCreateSummarizerClient create_summarizer_client;
  GetAIManagerRemote()->CreateSummarizer(
      create_summarizer_client.BindNewPipeAndPassRemote(), GetDefaultOptions());
  EXPECT_EQ(observer.WaitForBadMessage(), "Policy or user setting disabled");
  SetGenAILocalEnterprisePolicy(true);
}

TEST_F(AISummarizerTest, CreateOnDeviceAiUserSettingDisabled) {
  SetOnDeviceAiUserSetting(false);
  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  GetAIManagerInterface()->CanCreateSummarizer(GetDefaultOptions(),
                                               future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                              kUnavailableFeatureNotEnabled);

  mojo::test::BadMessageObserver observer;
  TestCreateSummarizerClient create_summarizer_client;
  GetAIManagerRemote()->CreateSummarizer(
      create_summarizer_client.BindNewPipeAndPassRemote(), GetDefaultOptions());
  EXPECT_EQ(observer.WaitForBadMessage(), "Policy or user setting disabled");
  SetOnDeviceAiUserSetting(true);
}

TEST_F(AISummarizerTest, DynamicConstraints) {
  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig config =
      CreateConfig();

  optimization_guide::proto::SummarizeMetadata metadata;
  metadata.mutable_constraints()->mutable_tldr_constraint()->set_regex(
      "^TLDR:.*");

  auto* feature_metadata = config.mutable_feature_metadata();
  feature_metadata->set_type_url(
      "type.googleapis.com/optimization_guide.proto.SummarizeMetadata");
  feature_metadata->set_value(metadata.SerializeAsString());

  optimization_guide::FakeAdaptationAsset fake_asset({.config = config});
  fake_broker_->UpdateModelAdaptation(fake_asset);

  fake_broker_->settings().set_execute_result({"TLDR: Result text"});

  auto options = GetDefaultOptions();
  options->type = blink::mojom::AISummarizerType::kTLDR;
  mojo::Remote<blink::mojom::AISummarizer> summarizer_remote =
      GetAISummarizerRemote(std::move(options));

  EXPECT_THAT(
      Summarize(*summarizer_remote, kInputString, kContextString),
      ElementsAreArray({"Hint: constrained_decoding ",
                        "Constraint: regex ^TLDR:.*", "TLDR: Result text"}));
}

TEST_F(AISummarizerTest, NoConstraints) {
  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig config =
      CreateConfig();

  optimization_guide::proto::SummarizeMetadata metadata;

  auto* feature_metadata = config.mutable_feature_metadata();
  feature_metadata->set_type_url(
      "type.googleapis.com/optimization_guide.proto.SummarizeMetadata");
  feature_metadata->set_value(metadata.SerializeAsString());

  optimization_guide::FakeAdaptationAsset fake_asset({.config = config});
  fake_broker_->UpdateModelAdaptation(fake_asset);

  fake_broker_->settings().set_execute_result({"Result text"});

  mojo::Remote<blink::mojom::AISummarizer> summarizer_remote =
      GetAISummarizerRemote(GetDefaultOptions());

  EXPECT_THAT(Summarize(*summarizer_remote, kInputString, kContextString),
              ElementsAreArray({"Result text"}));
}

TEST_F(AISummarizerTest, NoMetadata) {
  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig config =
      CreateConfig();

  optimization_guide::FakeAdaptationAsset fake_asset({.config = config});
  fake_broker_->UpdateModelAdaptation(fake_asset);

  fake_broker_->settings().set_execute_result({"Result text"});

  mojo::Remote<blink::mojom::AISummarizer> summarizer_remote =
      GetAISummarizerRemote(GetDefaultOptions());

  EXPECT_THAT(Summarize(*summarizer_remote, kInputString, kContextString),
              ElementsAreArray({"Result text"}));
}

class AISummarizerManifestTest : public AITestUtils::AITestManifestBase {
 public:
  AISummarizerManifestTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kAISummarizationPerformancePreference, {}},
         {optimization_guide::kOptimizationGuideManifestBroker, {}},
         {on_device_model::features::kOnDeviceModelLitertLmBackend, {}}},
        {});
  }

 protected:
  void SetupManifest() override {
    optimization_guide::proto::SummarizerFeatureConfig summarizer_cfg;
    summarizer_cfg.set_default_use_case("summarizer_api");
    (*summarizer_cfg.mutable_preference_use_cases())["speed"] =
        "summarizer_small_expert_model";
    (*summarizer_cfg.mutable_preference_use_cases())["capability"] =
        "summarizer_api";

    optimization_guide::proto::Any any_cfg;
    any_cfg.set_type_url(
        "type.googleapis.com/"
        "optimization_guide.proto.SummarizerFeatureConfig");
    any_cfg.set_value(summarizer_cfg.SerializeAsString());

    constexpr uint32_t kTestMaxTokens = 100u;

    optimization_guide::proto::SolutionConfig solution_config;
    *solution_config.mutable_feature() = CreateConfig();
    solution_config.mutable_safety()->set_feature(
        optimization_guide::proto::ModelExecutionFeature::
            MODEL_EXECUTION_FEATURE_SUMMARIZE);

    optimization_guide::ScenarioBuilder(
        fake_manifest_broker_->component_state())
        .AddBaseModel(
            "summarizer_api_solution",
            optimization_guide::BaseModelRecipeArgs(
                optimization_guide::proto::BaseModelRecipe::BACKEND_TYPE_GPU,
                optimization_guide::proto::BaseModelRecipe::
                    PERFORMANCE_HINT_HIGHEST_QUALITY,
                {}, kTestMaxTokens))
        .AddBaseModel(
            "summarizer_small_expert_model_solution",
            optimization_guide::BaseModelRecipeArgs(
                optimization_guide::proto::BaseModelRecipe::BACKEND_TYPE_CPU,
                optimization_guide::proto::BaseModelRecipe::
                    PERFORMANCE_HINT_UNSPECIFIED,
                {}, kTestMaxTokens))
        .AddSafetyModel("safety")
        .AddSafeSolution("summarizer_api", "summarizer_api_solution", "safety",
                         solution_config)
        .AddSafeSolution("summarizer_small_expert_model",
                         "summarizer_small_expert_model_solution", "safety",
                         solution_config)
        .SetFeatureConfig(optimization_guide::DeviceCategory::kGpuHighTier,
                          "summarizer_api", any_cfg)
        .Finish();

    fake_manifest_broker_->settings().performance_class =
        on_device_model::mojom::PerformanceClass::kHigh;
  }

  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig CreateConfig()
      override {
    optimization_guide::proto::OnDeviceModelExecutionFeatureConfig config;
    config.set_feature(optimization_guide::proto::ModelExecutionFeature::
                           MODEL_EXECUTION_FEATURE_SUMMARIZE);
    return config;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AISummarizerManifestTest,
       CanCreateAndCreateWithManifestSpeedPreference) {
  auto options = GetDefaultOptions();
  options->preference = blink::mojom::PerformancePreference::kSpeed;
  options->output_language = blink::mojom::AILanguageCode::New("en");

  fake_manifest_broker_->client().RequestAssetsFor(
      "summarizer_small_expert_model");

  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  GetAIManagerInterface()->CanCreateSummarizer(options.Clone(),
                                               future.GetCallback());
  EXPECT_EQ(future.Get(),
            blink::mojom::ModelAvailabilityCheckResult::kAvailable);

  TestCreateSummarizerClient summarizer_client;
  GetAIManagerRemote()->CreateSummarizer(
      summarizer_client.BindNewPipeAndPassRemote(), std::move(options));

  auto result = summarizer_client.result().Take();
  EXPECT_TRUE(result.has_value());
}

TEST_F(AISummarizerManifestTest, CanCreateSummarizerDownloadable) {
  auto options = GetDefaultOptions();
  options->preference = blink::mojom::PerformancePreference::kSpeed;
  options->output_language = blink::mojom::AILanguageCode::New("en");

  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  GetAIManagerInterface()->CanCreateSummarizer(options.Clone(),
                                               future.GetCallback());
  EXPECT_EQ(future.Get(),
            blink::mojom::ModelAvailabilityCheckResult::kDownloadable);
}

TEST_F(AISummarizerManifestTest, CanCreateAndCreateWithManifestAutoPreference) {
  fake_manifest_broker_->client().RequestAssetsFor("summarizer_api");

  auto options = GetDefaultOptions();
  options->output_language = blink::mojom::AILanguageCode::New("en");

  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  GetAIManagerInterface()->CanCreateSummarizer(options.Clone(),
                                               future.GetCallback());
  EXPECT_EQ(future.Get(),
            blink::mojom::ModelAvailabilityCheckResult::kAvailable);

  TestCreateSummarizerClient summarizer_client;
  GetAIManagerRemote()->CreateSummarizer(
      summarizer_client.BindNewPipeAndPassRemote(), std::move(options));

  auto result = summarizer_client.result().Take();
  EXPECT_TRUE(result.has_value());
}

TEST_F(AISummarizerManifestTest,
       CanCreateAndCreateWithManifestCapabilityPreference) {
  fake_manifest_broker_->client().RequestAssetsFor("summarizer_api");

  auto options = GetDefaultOptions();
  options->preference = blink::mojom::PerformancePreference::kCapability;
  options->output_language = blink::mojom::AILanguageCode::New("en");

  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  GetAIManagerInterface()->CanCreateSummarizer(options.Clone(),
                                               future.GetCallback());
  EXPECT_EQ(future.Get(),
            blink::mojom::ModelAvailabilityCheckResult::kAvailable);

  TestCreateSummarizerClient summarizer_client;
  GetAIManagerRemote()->CreateSummarizer(
      summarizer_client.BindNewPipeAndPassRemote(), std::move(options));

  auto result = summarizer_client.result().Take();
  EXPECT_TRUE(result.has_value());
}

TEST_F(AISummarizerManifestTest,
       CanCreateIncompatibleOptionsForSpeedPreference) {
  // Incompatible because speed preference requires kShort or kMedium length,
  // but we use kLong.
  auto options = GetDefaultOptions();
  options->preference = blink::mojom::PerformancePreference::kSpeed;
  options->length = blink::mojom::AISummarizerLength::kLong;

  base::MockCallback<AIManager::CanCreateSummarizerCallback> callback;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::
                      kUnavailableUnsupportedOptionsForPerformancePreference));

  GetAIManagerInterface()->CanCreateSummarizer(std::move(options),
                                               callback.Get());
}

TEST_F(AISummarizerManifestTest,
       CanCreateIncompatibleFormatForSpeedPreference) {
  auto options = GetDefaultOptions();
  options->preference = blink::mojom::PerformancePreference::kSpeed;
  options->format = blink::mojom::AISummarizerFormat::kMarkDown;

  base::MockCallback<AIManager::CanCreateSummarizerCallback> callback;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::
                      kUnavailableUnsupportedOptionsForPerformancePreference));

  GetAIManagerInterface()->CanCreateSummarizer(std::move(options),
                                               callback.Get());
}

TEST_F(AISummarizerManifestTest, CanCreateIncompatibleTypeForSpeedPreference) {
  auto options = GetDefaultOptions();
  options->preference = blink::mojom::PerformancePreference::kSpeed;
  options->type = blink::mojom::AISummarizerType::kTeaser;

  base::MockCallback<AIManager::CanCreateSummarizerCallback> callback;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::
                      kUnavailableUnsupportedOptionsForPerformancePreference));

  GetAIManagerInterface()->CanCreateSummarizer(std::move(options),
                                               callback.Get());
}

TEST_F(AISummarizerManifestTest,
       CanCreateIncompatibleOutputLanguageForSpeedPreference) {
  auto options = GetDefaultOptions();
  options->preference = blink::mojom::PerformancePreference::kSpeed;
  options->output_language = blink::mojom::AILanguageCode::New("fr");

  base::MockCallback<AIManager::CanCreateSummarizerCallback> callback;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::
                      kUnavailableUnsupportedOptionsForPerformancePreference));

  GetAIManagerInterface()->CanCreateSummarizer(std::move(options),
                                               callback.Get());
}

TEST_F(AISummarizerManifestTest,
       CanCreateIncompatibleInputLanguageForSpeedPreference) {
  auto options = GetDefaultOptions();
  options->preference = blink::mojom::PerformancePreference::kSpeed;
  options->expected_input_languages = AITestUtils::ToMojoLanguageCodes({"fr"});

  base::MockCallback<AIManager::CanCreateSummarizerCallback> callback;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::
                      kUnavailableUnsupportedOptionsForPerformancePreference));

  GetAIManagerInterface()->CanCreateSummarizer(std::move(options),
                                               callback.Get());
}

TEST_F(AISummarizerManifestTest,
       CanCreateIncompatibleContextLanguageForSpeedPreference) {
  auto options = GetDefaultOptions();
  options->preference = blink::mojom::PerformancePreference::kSpeed;
  options->expected_context_languages =
      AITestUtils::ToMojoLanguageCodes({"fr"});

  base::MockCallback<AIManager::CanCreateSummarizerCallback> callback;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::
                      kUnavailableUnsupportedOptionsForPerformancePreference));

  GetAIManagerInterface()->CanCreateSummarizer(std::move(options),
                                               callback.Get());
}

TEST_F(AISummarizerManifestTest,
       CanCreateIncompatibleSharedContextForSpeedPreference) {
  auto options = GetDefaultOptions();
  options->preference = blink::mojom::PerformancePreference::kSpeed;
  options->shared_context = "non-empty context";

  base::MockCallback<AIManager::CanCreateSummarizerCallback> callback;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::
                      kUnavailableUnsupportedOptionsForPerformancePreference));

  GetAIManagerInterface()->CanCreateSummarizer(std::move(options),
                                               callback.Get());
}

TEST_F(AISummarizerManifestTest,
       CanCreateSummarizerSpeedPreferenceFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {}, {blink::features::kAISummarizationPerformancePreference});

  auto options = GetDefaultOptions();
  options->preference = blink::mojom::PerformancePreference::kSpeed;

  mojo::test::BadMessageObserver observer;
  GetAIManagerRemote()->CanCreateSummarizer(std::move(options),
                                            base::DoNothing());
  EXPECT_EQ(observer.WaitForBadMessage(),
            "Speed preference requested but feature disabled");
}

TEST_F(AISummarizerManifestTest,
       CanCreateSummarizerNoServiceWithManifestBroker) {
  SetupNullOptimizationGuideKeyedService();

  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  GetAIManagerInterface()->CanCreateSummarizer(GetDefaultOptions(),
                                               future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                              kUnavailableFeatureExecutionNotEnabled);
}

TEST_F(AISummarizerManifestTest, CreateIncompatibleOptionsForSpeedPreference) {
  // Incompatible because speed preference requires kShort or kMedium length,
  // but we use kLong.
  auto options = GetDefaultOptions();
  options->preference = blink::mojom::PerformancePreference::kSpeed;
  options->length = blink::mojom::AISummarizerLength::kLong;

  TestCreateSummarizerClient create_summarizer_client;
  GetAIManagerRemote()->CreateSummarizer(
      create_summarizer_client.BindNewPipeAndPassRemote(), std::move(options));

  CreateSummarizerResult result = create_summarizer_client.result().Take();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::
                kUnsupportedOptionsForPerformancePreference);
}

TEST_F(AISummarizerManifestTest, SummarizeWithSpeedPreferenceAndContextFails) {
  auto options = GetDefaultOptions();
  options->preference = blink::mojom::PerformancePreference::kSpeed;
  options->output_language = blink::mojom::AILanguageCode::New("en");

  fake_manifest_broker_->client().RequestAssetsFor(
      "summarizer_small_expert_model");

  TestCreateSummarizerClient summarizer_client;
  GetAIManagerRemote()->CreateSummarizer(
      summarizer_client.BindNewPipeAndPassRemote(), std::move(options));

  auto result = summarizer_client.result().Take();
  ASSERT_TRUE(result.has_value());

  mojo::Remote<blink::mojom::AISummarizer> summarizer_remote(
      std::move(result.value()));

  AITestUtils::TestStreamingResponder responder;
  summarizer_remote->Summarize("input", "non-empty context",
                               responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorInvalidRequest);
}

}  // namespace

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_writer.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/version_info/channel.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "chrome/browser/ai/features.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/common/channel_info.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/scenario_builder.h"
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

using CreateWriterResult =
    base::expected<mojo::PendingRemote<blink::mojom::AIWriter>, Error>;

class TestCreateWriterClient
    : public blink::mojom::AIManagerCreateWriterClient {
 public:
  TestCreateWriterClient() = default;
  ~TestCreateWriterClient() override = default;
  TestCreateWriterClient(const TestCreateWriterClient&) = delete;
  TestCreateWriterClient& operator=(const TestCreateWriterClient&) = delete;

  mojo::PendingRemote<blink::mojom::AIManagerCreateWriterClient>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnResult(mojo::PendingRemote<::blink::mojom::AIWriter> writer) override {
    result_.SetValue(std::move(writer));
  }

  void OnError(blink::mojom::AIManagerCreateClientError error,
               blink::mojom::QuotaErrorInfoPtr quota_error_info) override {
    result_.SetValue(
        base::unexpected(Error{error, std::move(quota_error_info)}));
  }

  TestFuture<CreateWriterResult>& result() { return result_; }

 private:
  TestFuture<CreateWriterResult> result_;
  mojo::Receiver<blink::mojom::AIManagerCreateWriterClient> receiver_{this};
};

blink::mojom::AIWriterCreateOptionsPtr GetDefaultOptions() {
  return blink::mojom::AIWriterCreateOptions::New(
      kSharedContextString, blink::mojom::AIWriterTone::kNeutral,
      blink::mojom::AIWriterFormat::kPlainText,
      blink::mojom::AIWriterLength::kMedium,
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
        ProtoField({WritingAssistanceApiRequest::kInstructionsFieldNumber})));
  }

  return safety_config;
}

optimization_guide::proto::OnDeviceModelExecutionFeatureConfig
CreateWriterConfig() {
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
      ProtoField({WritingAssistanceApiRequest::kInstructionsFieldNumber}));

  auto& output_config = *config.mutable_output_config();
  output_config.set_proto_type(WritingAssistanceApiResponse().GetTypeName());
  *output_config.mutable_proto_field() = optimization_guide::ProtoField({1});

  return config;
}

class AIWriterTest : public AITestUtils::AITestBase {
 protected:
  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig CreateConfig()
      override {
    return CreateWriterConfig();
  }

  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig
  CreateSafeConfig() {
    auto config = CreateConfig();
    config.set_can_skip_text_safety(false);
    return config;
  }

  mojo::Remote<blink::mojom::AIWriter> GetAIWriterRemote(
      blink::mojom::AIWriterCreateOptionsPtr options = GetDefaultOptions()) {
    TestCreateWriterClient create_writer_client;
    GetAIManagerRemote()->CreateWriter(
        create_writer_client.BindNewPipeAndPassRemote(), std::move(options),
        /*monitor=*/mojo::NullRemote());

    CreateWriterResult result = create_writer_client.result().Take();
    EXPECT_OK(result);
    return mojo::Remote<blink::mojom::AIWriter>(std::move(result.value()));
  }

  void RunSimpleWriteTest(blink::mojom::AIWriterTone tone,
                          blink::mojom::AIWriterFormat format,
                          blink::mojom::AIWriterLength length) {
    fake_broker_->settings().set_execute_result({"Result text"});

    const auto options = blink::mojom::AIWriterCreateOptions::New(
        kSharedContextString, tone, format, length,
        /*expected_input_languages=*/std::vector<AILanguageCodePtr>(),
        /*expected_context_languages=*/std::vector<AILanguageCodePtr>(),
        /*output_language=*/AILanguageCode::New(""));

    mojo::Remote<blink::mojom::AIWriter> writer_remote =
        GetAIWriterRemote(options.Clone());

    EXPECT_THAT(Write(*writer_remote, kInputString, kContextString),
                ElementsAreArray({"Result text"}));
  }

  std::vector<std::string> Write(blink::mojom::AIWriter& writer,
                                 const std::string& input,
                                 const std::string& context) {
    AITestUtils::TestStreamingResponder responder;
    writer.Write(input, context, responder.BindRemote());
    EXPECT_TRUE(responder.WaitForCompletion());
    // Return Writer's response without the final empty string chunk.
    return responder.responses_without_last();
  }

  void EnsureModelIsReady() {
    TestCreateWriterClient writer_client;
    GetAIManagerRemote()->CreateWriter(writer_client.BindNewPipeAndPassRemote(),
                                       GetDefaultOptions(),
                                       /*monitor=*/mojo::NullRemote());

    auto result = writer_client.result().Take();
    EXPECT_OK(result);
  }
};

TEST_F(AIWriterTest, CanCreateDefaultOptions) {
  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    GetAIManagerInterface()->CanCreateWriter(GetDefaultOptions(),
                                             future.GetCallback());
    EXPECT_EQ(future.Get(),
              blink::mojom::ModelAvailabilityCheckResult::kDownloadable);
  }

  // After model is ready, `CanCreateWriter` should return available.
  EnsureModelIsReady();

  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    GetAIManagerInterface()->CanCreateWriter(GetDefaultOptions(),
                                             future.GetCallback());
    EXPECT_EQ(future.Get(),
              blink::mojom::ModelAvailabilityCheckResult::kAvailable);
  }
}

TEST_F(AIWriterTest, CanCreateIsLanguagesSupported) {
  EnsureModelIsReady();

  auto options = GetDefaultOptions();
  options->output_language = AILanguageCode::New("en");
  options->expected_input_languages =
      AITestUtils::ToMojoLanguageCodes({"en-US", ""});
  options->expected_context_languages =
      AITestUtils::ToMojoLanguageCodes({"en-GB", ""});

  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  GetAIManagerInterface()->CanCreateWriter(std::move(options),
                                           future.GetCallback());
  EXPECT_EQ(future.Get(),
            blink::mojom::ModelAvailabilityCheckResult::kAvailable);
}

TEST_F(AIWriterTest, CanCreateUnIsLanguagesSupported) {
  auto options = GetDefaultOptions();
  options->output_language = AILanguageCode::New("es-ES");
  options->expected_input_languages =
      AITestUtils::ToMojoLanguageCodes({"en", "tlh", "ja"});
  options->expected_context_languages =
      AITestUtils::ToMojoLanguageCodes({"ar", "zh", "hi"});
  base::MockCallback<AIManager::CanCreateWriterCallback> callback;
  EXPECT_CALL(callback, Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage));
  GetAIManagerInterface()->CanCreateWriter(std::move(options), callback.Get());
}

TEST_F(AIWriterTest, ToProtoOptionsLanguagesSupported) {
  // Writer proto expects base language display names in English.
  std::vector<std::pair<std::string, std::string>> languages = {
      {"en", "English"},  {"en-us", "English"},  {"en-uk", "English"},
      {"es", "Spanish"},  {"es-sp", "Spanish"},  {"es-mx", "Spanish"},
      {"ja", "Japanese"}, {"ja-jp", "Japanese"}, {"ja-foo", "Japanese"},
  };
  blink::mojom::AIWriterCreateOptionsPtr options = GetDefaultOptions();
  for (const auto& language : languages) {
    options->output_language = AILanguageCode::New(language.first);
    const auto proto_options = AIWriter::ToProtoOptions(options);
    EXPECT_EQ(proto_options->output_language(), language.second);
  }
}

TEST_F(AIWriterTest, CreateWriterNoService) {
  SetupNullOptimizationGuideKeyedService();

  TestCreateWriterClient create_writer_client;
  GetAIManagerRemote()->CreateWriter(
      create_writer_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());

  CreateWriterResult result = create_writer_client.result().Take();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

TEST_F(AIWriterTest, WriterTelemetry) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      GetOnDeviceModelEligibility(
          optimization_guide::mojom::OnDeviceFeature::kWritingAssistanceApi))
      .WillRepeatedly(testing::Return(
          optimization_guide::OnDeviceModelEligibilityReason::kSuccess));
  EnsureModelIsReady();
  GetAIWriterRemote();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution."
      "OnDeviceModelEligibilityReason.WritingAssistanceApi",
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess, 2);
}

TEST_F(AIWriterTest, CreateWriterModelNotEligible) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{optimization_guide::features::kOnDeviceModelPerformanceParams,
        {{"compatible_on_device_performance_classes", "3,4,5,6"}}}},
      {{on_device_model::features::kOnDeviceModelCpuBackend}});

  fake_broker_->service_settings().performance_class =
      PerformanceClass::kVeryLow;

  TestCreateWriterClient create_writer_client;
  GetAIManagerRemote()->CreateWriter(
      create_writer_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());

  CreateWriterResult result = create_writer_client.result().Take();
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

TEST_F(AIWriterTest, CreateWriterWaitsForBaseModel) {
  fake_broker_->InstallBaseModel(nullptr);

  TestCreateWriterClient create_writer_client;
  GetAIManagerRemote()->CreateWriter(
      create_writer_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());

  TestFuture<CreateWriterResult>& future = create_writer_client.result();
  task_environment()->FastForwardBy(base::Hours(1));
  EXPECT_FALSE(future.IsReady());

  fake_broker_->InstallBaseModel(
      std::make_unique<optimization_guide::FakeBaseModelAsset>());

  EXPECT_OK(future.Take());
}

TEST_F(AIWriterTest, CreateWriterWaitsForModelAdaptation) {
  fake_broker_->model_provider().RemoveModel(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_WRITING_ASSISTANCE_API);

  TestCreateWriterClient create_writer_client;
  GetAIManagerRemote()->CreateWriter(
      create_writer_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());

  TestFuture<CreateWriterResult>& future = create_writer_client.result();
  task_environment()->FastForwardBy(base::Hours(1));
  EXPECT_FALSE(future.IsReady());

  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);

  EXPECT_OK(future.Take());
}

TEST_F(AIWriterTest, CreateWriterWaitsForTextSafetyModel) {
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);

  TestCreateWriterClient create_writer_client;
  GetAIManagerRemote()->CreateWriter(
      create_writer_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());

  TestFuture<CreateWriterResult>& future = create_writer_client.result();
  task_environment()->FastForwardBy(base::Hours(1));
  EXPECT_FALSE(future.IsReady());

  optimization_guide::FakeSafetyModelAsset safety_asset(CreateSafetyConfig());
  fake_broker_->UpdateSafetyModel(safety_asset);

  EXPECT_OK(future.Take());
}

TEST_F(AIWriterTest, CreateWriterSafetyConfigNotAvailable) {
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);
  // Provide a safety asset that does not support writer.
  optimization_guide::FakeSafetyModelAsset safety_asset([] {
    auto safety_config = CreateSafetyConfig();
    safety_config.set_feature(
        optimization_guide::proto::MODEL_EXECUTION_FEATURE_TEST);
    return safety_config;
  }());
  fake_broker_->UpdateSafetyModel(safety_asset);

  TestCreateWriterClient create_writer_client;
  GetAIManagerRemote()->CreateWriter(
      create_writer_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());

  CreateWriterResult result = create_writer_client.result().Take();
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

TEST_F(AIWriterTest, CreateWriterUnableToCalculateTokenSize) {
  // Incorrect `request_base_name` cause session to fail constructing input
  // string and checking token size.
  auto config = CreateConfig();
  auto& input_config = *config.mutable_input_config();
  input_config.set_request_base_name("InvalidRequestBaseName");

  optimization_guide::FakeAdaptationAsset fake_asset({.config = config});
  fake_broker_->UpdateModelAdaptation(fake_asset);

  TestCreateWriterClient create_writer_client;
  GetAIManagerRemote()->CreateWriter(
      create_writer_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());

  CreateWriterResult result = create_writer_client.result().Take();
  EXPECT_EQ(
      result.error().error,
      blink::mojom::AIManagerCreateClientError::kUnableToCalculateTokenSize);
}

TEST_F(AIWriterTest, CreateWriterContextLimitExceededError) {
  fake_broker_->settings().set_size_in_tokens(
      blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);

  TestCreateWriterClient create_writer_client;
  GetAIManagerRemote()->CreateWriter(
      create_writer_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());

  CreateWriterResult result = create_writer_client.result().Take();
  EXPECT_EQ(result.error().error,
            blink::mojom::AIManagerCreateClientError::kInitialInputTooLarge);
  EXPECT_EQ(result.error().quota_error_info->requested,
            blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);
  EXPECT_EQ(result.error().quota_error_info->quota,
            blink::mojom::kWritingAssistanceMaxInputTokenSize);
}

TEST_F(AIWriterTest, WriteDefault) {
  RunSimpleWriteTest(blink::mojom::AIWriterTone::kNeutral,
                     blink::mojom::AIWriterFormat::kPlainText,
                     blink::mojom::AIWriterLength::kMedium);
}

TEST_F(AIWriterTest, WriteWithOptions) {
  blink::mojom::AIWriterTone tones[]{
      blink::mojom::AIWriterTone::kFormal,
      blink::mojom::AIWriterTone::kNeutral,
      blink::mojom::AIWriterTone::kCasual,
  };
  blink::mojom::AIWriterFormat formats[]{
      blink::mojom::AIWriterFormat::kPlainText,
      blink::mojom::AIWriterFormat::kMarkdown,
  };
  blink::mojom::AIWriterLength lengths[]{
      blink::mojom::AIWriterLength::kShort,
      blink::mojom::AIWriterLength::kMedium,
      blink::mojom::AIWriterLength::kLong,
  };
  for (const auto& tone : tones) {
    for (const auto& format : formats) {
      for (const auto& length : lengths) {
        SCOPED_TRACE(testing::Message()
                     << tone << " " << format << " " << length);
        RunSimpleWriteTest(tone, format, length);
      }
    }
  }
}

TEST_F(AIWriterTest, InputLimitExceededError) {
  auto writer_remote = GetAIWriterRemote();

  fake_broker_->settings().set_size_in_tokens(
      blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);

  AITestUtils::TestStreamingResponder responder;
  writer_remote->Write(kInputString, kContextString, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorInputTooLarge);
  ASSERT_EQ(responder.quota_error_info().requested,
            blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);
  ASSERT_EQ(responder.quota_error_info().quota,
            blink::mojom::kWritingAssistanceMaxInputTokenSize);
}

TEST_F(AIWriterTest, WriteMultipleResponse) {
  auto writer_remote = GetAIWriterRemote();

  std::vector<std::string> result = {"Result ", "text"};
  fake_broker_->settings().set_execute_result(result);
  EXPECT_THAT(Write(*writer_remote, kInputString, kContextString),
              ElementsAreArray(result));
}

TEST_F(AIWriterTest, MultipleWrite) {
  auto writer_remote = GetAIWriterRemote();

  std::vector<std::string> result = {"Result ", "text"};
  fake_broker_->settings().set_execute_result(result);
  EXPECT_THAT(Write(*writer_remote, kInputString, kContextString),
              ElementsAreArray(result));

  std::vector<std::string> result2 = {"Result ", "text ", "2"};
  fake_broker_->settings().set_execute_result(result2);
  EXPECT_THAT(Write(*writer_remote, "input string 2", "test context 2"),
              ElementsAreArray(result2));
}

TEST_F(AIWriterTest, MeasureUsage) {
  auto writer_remote = GetAIWriterRemote();

  base::test::TestFuture<std::optional<uint32_t>> future;
  writer_remote->MeasureUsage(kInputString, kContextString,
                              future.GetCallback());

  auto size = std::string(kSharedContextString).size() +
              std::string(kContextString).size() +
              std::string(kInputString).size();
  EXPECT_EQ(future.Get(), size);
}

TEST_F(AIWriterTest, Priority) {
  fake_broker_->settings().set_execute_result({"hi"});
  auto writer_remote = GetAIWriterRemote();

  EXPECT_THAT(Write(*writer_remote, kInputString, kContextString),
              ElementsAre("hi"));

  web_contents()->WasHidden();
  EXPECT_THAT(Write(*writer_remote, kInputString, kContextString),
              ElementsAre("Priority: background", "hi"));

  web_contents()->WasShown();
  EXPECT_THAT(Write(*writer_remote, kInputString, kContextString),
              ElementsAre("hi"));
}

TEST_F(AIWriterTest, TextSafetyInput) {
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);
  optimization_guide::FakeSafetyModelAsset safety_asset(CreateSafetyConfig());
  fake_broker_->UpdateSafetyModel(safety_asset);

  fake_broker_->settings().set_execute_result({"hi"});
  auto writer_remote = GetAIWriterRemote();
  EXPECT_THAT(Write(*writer_remote, kInputString, kContextString),
              ElementsAre("hi"));

  AITestUtils::TestStreamingResponder responder;
  writer_remote->Write("unsafe", kContextString, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
}

TEST_F(AIWriterTest, TextSafetyContext) {
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);
  optimization_guide::FakeSafetyModelAsset safety_asset(CreateSafetyConfig());
  fake_broker_->UpdateSafetyModel(safety_asset);

  fake_broker_->settings().set_execute_result({"hi"});
  auto writer_remote = GetAIWriterRemote();
  EXPECT_THAT(Write(*writer_remote, kInputString, kContextString),
              ElementsAre("hi"));

  AITestUtils::TestStreamingResponder responder;
  writer_remote->Write(kInputString, "unsafe", responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
}

TEST_F(AIWriterTest, TextSafetySharedContext) {
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);
  optimization_guide::FakeSafetyModelAsset safety_asset(CreateSafetyConfig());
  fake_broker_->UpdateSafetyModel(safety_asset);

  const auto options = blink::mojom::AIWriterCreateOptions::New(
      "unsafe", blink::mojom::AIWriterTone::kNeutral,
      blink::mojom::AIWriterFormat::kPlainText,
      blink::mojom::AIWriterLength::kMedium,
      /*expected_input_languages=*/std::vector<AILanguageCodePtr>(),
      /*expected_context_languages=*/std::vector<AILanguageCodePtr>(),
      /*output_language=*/AILanguageCode::New(""));

  mojo::Remote<blink::mojom::AIWriter> writer_remote =
      GetAIWriterRemote(options.Clone());
  AITestUtils::TestStreamingResponder responder;
  writer_remote->Write(kInputString, kContextString, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
}

TEST_F(AIWriterTest, TextSafetyOutput) {
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
  auto writer_remote = GetAIWriterRemote();
  AITestUtils::TestStreamingResponder responder;
  writer_remote->Write(kInputString, kContextString, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
  EXPECT_TRUE(responder.responses().empty());
}

TEST_F(AIWriterTest, TextSafetyOutputPartial) {
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
  auto writer_remote = GetAIWriterRemote();
  AITestUtils::TestStreamingResponder responder;
  writer_remote->Write(kInputString, kContextString, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
  // Partial checks should still allow some output to stream.
  EXPECT_THAT(responder.responses(), ElementsAre("abc", "de", "fg"));
}

TEST_F(AIWriterTest, ServiceCrash) {
  fake_broker_->settings().set_execute_result({"hi"});

  auto writer_remote = GetAIWriterRemote();
  AITestUtils::TestStreamingResponder responder;
  writer_remote->Write(kInputString, kContextString, responder.BindRemote());
  fake_broker_->CrashService();

  EXPECT_FALSE(responder.WaitForCompletion());
  // TODO(crbug.com/494980521): Crashes should be yield kErrorSessionDestroyed.
  EXPECT_EQ(
      responder.error_status(),
      blink::mojom::ModelStreamingResponseStatus::kErrorFailedToCountTokens);

  writer_remote = GetAIWriterRemote();
  EXPECT_THAT(Write(*writer_remote, kInputString, kContextString),
              ElementsAre("hi"));
}

TEST_F(AIWriterTest, CrashRecoveryMeasureInputUsage) {
  auto writer_remote = GetAIWriterRemote();
  fake_broker_->CrashService();

  base::test::TestFuture<std::optional<uint32_t>> measure_future;
  writer_remote->MeasureUsage(kInputString, kContextString,
                              measure_future.GetCallback());

  auto size = std::string(kSharedContextString).size() +
              std::string(kContextString).size() +
              std::string(kInputString).size();
  EXPECT_EQ(measure_future.Get(), size);
}

TEST_F(AIWriterTest, CanCreatePermissionsPolicyDisabled) {
  DisablePolicy(network::mojom::PermissionsPolicyFeature::kWriter);
  mojo::test::BadMessageObserver observer;
  GetAIManagerRemote()->CanCreateWriter(GetDefaultOptions(), base::DoNothing());
  EXPECT_EQ(observer.WaitForBadMessage(), "Permissions policy disabled");
}

TEST_F(AIWriterTest, CreatePermissionsPolicyDisabled) {
  DisablePolicy(network::mojom::PermissionsPolicyFeature::kWriter);
  mojo::test::BadMessageObserver observer;
  TestCreateWriterClient create_writer_client;
  GetAIManagerRemote()->CreateWriter(
      create_writer_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());
  EXPECT_EQ(observer.WaitForBadMessage(), "Policy or user setting disabled");
}

TEST_F(AIWriterTest, CreateBuiltInAIAPIsEnterprisePolicyDisabled) {
  SetBuiltInAIAPIsEnterprisePolicy(false);
  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  GetAIManagerInterface()->CanCreateWriter(GetDefaultOptions(),
                                           future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                              kUnavailableEnterprisePolicyDisabled);

  mojo::test::BadMessageObserver observer;
  TestCreateWriterClient create_writer_client;
  GetAIManagerRemote()->CreateWriter(
      create_writer_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());
  EXPECT_EQ(observer.WaitForBadMessage(), "Policy or user setting disabled");
  SetBuiltInAIAPIsEnterprisePolicy(true);
}

TEST_F(AIWriterTest, CreateGenAILocalEnterprisePolicyDisabled) {
  SetGenAILocalEnterprisePolicy(false);
  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  GetAIManagerInterface()->CanCreateWriter(GetDefaultOptions(),
                                           future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                              kUnavailableEnterprisePolicyDisabled);

  mojo::test::BadMessageObserver observer;
  TestCreateWriterClient create_writer_client;
  GetAIManagerRemote()->CreateWriter(
      create_writer_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());
  EXPECT_EQ(observer.WaitForBadMessage(), "Policy or user setting disabled");
  SetGenAILocalEnterprisePolicy(true);
}

TEST_F(AIWriterTest, CreateOnDeviceAiUserSettingDisabled) {
  SetOnDeviceAiUserSetting(false);
  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  GetAIManagerInterface()->CanCreateWriter(GetDefaultOptions(),
                                           future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                              kUnavailableFeatureNotEnabled);

  mojo::test::BadMessageObserver observer;
  TestCreateWriterClient create_writer_client;
  GetAIManagerRemote()->CreateWriter(
      create_writer_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());
  EXPECT_EQ(observer.WaitForBadMessage(), "Policy or user setting disabled");
  SetOnDeviceAiUserSetting(true);
}

class AIWriterManifestTest : public AITestUtils::AITestManifestBase {
 protected:
  AIWriterManifestTest() {
    scoped_feature_list_.InitWithFeatures(
        {optimization_guide::kOptimizationGuideManifestBroker,
         on_device_model::features::kOnDeviceModelLitertLmBackend},
        {});
  }

  void SetupManifest() override {
    optimization_guide::proto::WritingAssistanceApiFeatureConfig writer_cfg;
    writer_cfg.set_default_use_case("writing_assistance_api");
    (*writer_cfg.mutable_experimental_use_cases())["v4"] =
        "writing_assistance_gemma4";

    optimization_guide::proto::Any any_cfg;
    any_cfg.set_type_url(
        "type.googleapis.com/"
        "chrome_intelligence_proto_features.WritingAssistanceApiFeatureConfig");
    any_cfg.set_value(writer_cfg.SerializeAsString());

    optimization_guide::proto::SolutionConfig solution_config;
    *solution_config.mutable_feature() = CreateConfig();

    optimization_guide::ScenarioBuilder(
        fake_manifest_broker_->component_state())
        .AddBaseModel(
            "writing_assistance_base_model",
            optimization_guide::BaseModelRecipeArgs(
                optimization_guide::proto::BaseModelRecipe::BACKEND_TYPE_GPU,
                optimization_guide::proto::BaseModelRecipe::
                    PERFORMANCE_HINT_HIGHEST_QUALITY,
                {}, 8096))
        .AddBaseModel(
            "writing_assistance_gemma4_base_model",
            optimization_guide::BaseModelRecipeArgs(
                optimization_guide::proto::BaseModelRecipe::BACKEND_TYPE_GPU,
                optimization_guide::proto::BaseModelRecipe::
                    PERFORMANCE_HINT_HIGHEST_QUALITY,
                {}, 8096))
        .AddSafetyModel("safety_model")
        .AddSafeSolution("writing_assistance_api",
                         "writing_assistance_base_model", "safety_model",
                         solution_config)
        .AddSafeSolution("writing_assistance_gemma4",
                         "writing_assistance_gemma4_base_model", "safety_model",
                         solution_config)
        .SetFeatureConfig(optimization_guide::DeviceCategory::kGpuHighTier,
                          "writing_assistance_api", any_cfg)
        .Finish();

    fake_manifest_broker_->settings().performance_class =
        on_device_model::mojom::PerformanceClass::kHigh;
  }

  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig CreateConfig()
      override {
    return CreateWriterConfig();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AIWriterManifestTest, CanCreateAndCreateWithManifestGemma4) {
  version_info::Channel channel = chrome::GetChannel();
  if (channel != version_info::Channel::CANARY &&
      channel != version_info::Channel::DEV &&
      channel != version_info::Channel::UNKNOWN &&
      version_info::IsOfficialBuild()) {
    GTEST_SKIP() << "Experimental use case support is limited to "
                    "Canary/Dev/Unknown channels and unofficial builds.";
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kAIApiFoundationalModel, {{"model_version", "v4"}});

  ASSERT_TRUE(fake_manifest_broker_);
  fake_manifest_broker_->client().RequestAssetsFor("writing_assistance_gemma4");

  // Verify CanCreateWriter check passes successfully.
  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  ai_manager_->CanCreateWriter(GetDefaultOptions(), future.GetCallback());
  EXPECT_EQ(future.Get(),
            blink::mojom::ModelAvailabilityCheckResult::kAvailable);

  // Verify CreateWriter can retrieve the model successfully.
  TestCreateWriterClient create_writer_client;
  GetAIManagerRemote()->CreateWriter(
      create_writer_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      /*monitor=*/mojo::NullRemote());

  auto result = create_writer_client.result().Take();
  EXPECT_TRUE(result.has_value());
}

TEST_F(AIWriterManifestTest, CanCreateBeforeDownloadGemma4) {
  version_info::Channel channel = chrome::GetChannel();
  if (channel != version_info::Channel::CANARY &&
      channel != version_info::Channel::DEV &&
      channel != version_info::Channel::UNKNOWN &&
      version_info::IsOfficialBuild()) {
    GTEST_SKIP() << "Experimental use case support is limited to "
                    "Canary/Dev/Unknown channels and unofficial builds.";
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kAIApiFoundationalModel, {{"model_version", "v4"}});

  ASSERT_TRUE(fake_manifest_broker_);

  fake_manifest_broker_->client().RequestAssetsFor("writing_assistance_api");

  // Verify CanCreateWriter check returns kDownloadable before assets are
  // requested.
  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  ai_manager_->CanCreateWriter(GetDefaultOptions(), future.GetCallback());
  EXPECT_EQ(future.Get(),
            blink::mojom::ModelAvailabilityCheckResult::kDownloadable);
}

}  // namespace

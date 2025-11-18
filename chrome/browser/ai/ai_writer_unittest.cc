// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_writer.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/test_future.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "components/optimization_guide/core/model_execution/test/mock_on_device_capability.h"
#include "components/optimization_guide/core/model_execution/test/substitution_builder.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/writing_assistance_api.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "content/public/browser/render_widget_host_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace {

using ::blink::mojom::AILanguageCode;
using ::blink::mojom::AILanguageCodePtr;
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

class MockCreateWriterClient
    : public blink::mojom::AIManagerCreateWriterClient {
 public:
  MockCreateWriterClient() = default;
  ~MockCreateWriterClient() override = default;
  MockCreateWriterClient(const MockCreateWriterClient&) = delete;
  MockCreateWriterClient& operator=(const MockCreateWriterClient&) = delete;

  mojo::PendingRemote<blink::mojom::AIManagerCreateWriterClient>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void,
              OnResult,
              (mojo::PendingRemote<::blink::mojom::AIWriter> writer),
              (override));
  MOCK_METHOD(void,
              OnError,
              (blink::mojom::AIManagerCreateClientError error,
               blink::mojom::QuotaErrorInfoPtr quota_error_info),
              (override));

 private:
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

class AIWriterTest : public AITestUtils::AITestBase {
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
        ProtoField({WritingAssistanceApiRequest::kInstructionsFieldNumber}));

    auto& output_config = *config.mutable_output_config();
    output_config.set_proto_type(WritingAssistanceApiResponse().GetTypeName());
    *output_config.mutable_proto_field() = optimization_guide::ProtoField({1});

    return config;
  }

  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig
  CreateSafeConfig() {
    auto config = CreateConfig();
    config.set_can_skip_text_safety(false);
    return config;
  }

  mojo::Remote<blink::mojom::AIWriter> GetAIWriterRemote() {
    mojo::Remote<blink::mojom::AIWriter> writer_remote;

    MockCreateWriterClient mock_create_writer_client;
    base::RunLoop run_loop;
    EXPECT_CALL(mock_create_writer_client, OnResult(_))
        .WillOnce([&](mojo::PendingRemote<::blink::mojom::AIWriter> writer) {
          EXPECT_TRUE(writer);
          writer_remote =
              mojo::Remote<blink::mojom::AIWriter>(std::move(writer));
          run_loop.Quit();
        });

    mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
    ai_manager->CreateWriter(
        mock_create_writer_client.BindNewPipeAndPassRemote(),
        GetDefaultOptions());
    run_loop.Run();

    return writer_remote;
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
    mojo::Remote<blink::mojom::AIWriter> writer_remote;
    {
      MockCreateWriterClient mock_create_writer_client;
      base::RunLoop run_loop;
      EXPECT_CALL(mock_create_writer_client, OnResult(_))
          .WillOnce([&](mojo::PendingRemote<::blink::mojom::AIWriter> writer) {
            EXPECT_TRUE(writer);
            writer_remote =
                mojo::Remote<blink::mojom::AIWriter>(std::move(writer));
            run_loop.Quit();
          });

      mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
      ai_manager->CreateWriter(
          mock_create_writer_client.BindNewPipeAndPassRemote(),
          options.Clone());
      run_loop.Run();
    }

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
};

TEST_F(AIWriterTest, CanCreateDefaultOptions) {
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              GetOnDeviceModelEligibilityAsync(_, _, _))
      .WillOnce([](auto feature, auto capabilities, auto callback) {
        std::move(callback).Run(
            optimization_guide::OnDeviceModelEligibilityReason::kSuccess);
      });
  base::MockCallback<AIManager::CanCreateWriterCallback> callback;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::kAvailable));
  GetAIManagerInterface()->CanCreateWriter(GetDefaultOptions(), callback.Get());
}

TEST_F(AIWriterTest, CanCreateIsLanguagesSupported) {
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
  base::MockCallback<AIManager::CanCreateWriterCallback> callback;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::kAvailable));
  GetAIManagerInterface()->CanCreateWriter(std::move(options), callback.Get());
}

TEST_F(AIWriterTest, CanCreateUnIsLanguagesSupported) {
  auto options = GetDefaultOptions();
  options->output_language = AILanguageCode::New("es-ES");
  options->expected_input_languages =
      AITestUtils::ToMojoLanguageCodes({"en", "fr", "ja"});
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
  MockCreateWriterClient mock_create_writer_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_writer_client, OnError(_, _))
      .WillOnce([&](blink::mojom::AIManagerCreateClientError error,
                    blink::mojom::QuotaErrorInfoPtr quota_error_info) {
        ASSERT_EQ(
            error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
        run_loop.Quit();
      });

  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateWriter(mock_create_writer_client.BindNewPipeAndPassRemote(),
                           GetDefaultOptions());
  run_loop.Run();
}

TEST_F(AIWriterTest, CanCreateWaitsForEligibility) {
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
  GetAIManagerInterface()->CanCreateWriter(GetDefaultOptions(),
                                           result_future.GetCallback());
  // Session should not be ready until eligibility callback has run.
  EXPECT_FALSE(result_future.IsReady());
  eligibility_future.Take().Run(
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);
  EXPECT_EQ(result_future.Get(),
            blink::mojom::ModelAvailabilityCheckResult::kAvailable);
}

TEST_F(AIWriterTest, CanCreateUnavailableWhenAdaptationNotAvailable) {
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              GetOnDeviceModelEligibilityAsync(_, _, _))
      .WillOnce([&](auto feature, auto capabilities, auto callback) {
        std::move(callback).Run(
            optimization_guide::OnDeviceModelEligibilityReason::
                kModelAdaptationNotAvailable);
      });

  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult>
      result_future;
  GetAIManagerInterface()->CanCreateWriter(GetDefaultOptions(),
                                           result_future.GetCallback());
  EXPECT_EQ(result_future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                                     kUnavailableModelAdaptationNotAvailable);
}

TEST_F(AIWriterTest, CreateWriterUnableToCalculateTokenSize) {
  // Incorrect `request_base_name` cause session to fail constructing input
  // string and checking token size.
  auto config = CreateConfig();
  auto& input_config = *config.mutable_input_config();
  input_config.set_request_base_name("InvalidRequestBaseName");

  optimization_guide::FakeAdaptationAsset fake_asset({.config = config});
  fake_broker_->UpdateModelAdaptation(fake_asset);

  MockCreateWriterClient mock_create_writer_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_writer_client, OnError(_, _))
      .WillOnce([&](blink::mojom::AIManagerCreateClientError error,
                    blink::mojom::QuotaErrorInfoPtr quota_error_info) {
        ASSERT_EQ(error, blink::mojom::AIManagerCreateClientError::
                             kUnableToCalculateTokenSize);
        run_loop.Quit();
      });

  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateWriter(mock_create_writer_client.BindNewPipeAndPassRemote(),
                           GetDefaultOptions());
  run_loop.Run();
}

TEST_F(AIWriterTest, CreateWriterContextLimitExceededError) {
  fake_broker_->settings().set_size_in_tokens(
      blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);

  MockCreateWriterClient mock_create_writer_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_writer_client, OnError(_, _))
      .WillOnce([&](blink::mojom::AIManagerCreateClientError error,
                    blink::mojom::QuotaErrorInfoPtr quota_error_info) {
        ASSERT_EQ(
            error,
            blink::mojom::AIManagerCreateClientError::kInitialInputTooLarge);
        ASSERT_TRUE(quota_error_info);
        ASSERT_EQ(quota_error_info->requested,
                  blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);
        ASSERT_EQ(quota_error_info->quota,
                  blink::mojom::kWritingAssistanceMaxInputTokenSize);
        run_loop.Quit();
      });

  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateWriter(mock_create_writer_client.BindNewPipeAndPassRemote(),
                           GetDefaultOptions());
  run_loop.Run();
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

  main_rfh()->GetRenderWidgetHost()->GetView()->Hide();
  EXPECT_THAT(Write(*writer_remote, kInputString, kContextString),
              ElementsAre("Priority: background", "hi"));

  main_rfh()->GetRenderWidgetHost()->GetView()->Show();
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
  mojo::Remote<blink::mojom::AIWriter> writer_remote;
  {
    MockCreateWriterClient mock_create_writer_client;
    base::RunLoop run_loop;
    EXPECT_CALL(mock_create_writer_client, OnResult(_))
        .WillOnce([&](mojo::PendingRemote<::blink::mojom::AIWriter> writer) {
          EXPECT_TRUE(writer);
          writer_remote =
              mojo::Remote<blink::mojom::AIWriter>(std::move(writer));
          run_loop.Quit();
        });

    mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
    ai_manager->CreateWriter(
        mock_create_writer_client.BindNewPipeAndPassRemote(), options.Clone());
    run_loop.Run();
  }

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
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure);

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

}  // namespace

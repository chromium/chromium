// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_summarizer.h"

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
#include "components/optimization_guide/proto/features/summarize.pb.h"
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
using ::optimization_guide::proto::SummarizeRequest;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

constexpr char kSharedContextString[] = "test shared context";
constexpr char kContextString[] = "test context";
constexpr char kInputString[] = "input string";

class MockCreateSummarizerClient
    : public blink::mojom::AIManagerCreateSummarizerClient {
 public:
  MockCreateSummarizerClient() = default;
  ~MockCreateSummarizerClient() override = default;
  MockCreateSummarizerClient(const MockCreateSummarizerClient&) = delete;
  MockCreateSummarizerClient& operator=(const MockCreateSummarizerClient&) =
      delete;

  mojo::PendingRemote<blink::mojom::AIManagerCreateSummarizerClient>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void,
              OnResult,
              (mojo::PendingRemote<::blink::mojom::AISummarizer> Summarizer),
              (override));
  MOCK_METHOD(void,
              OnError,
              (blink::mojom::AIManagerCreateClientError error,
               blink::mojom::QuotaErrorInfoPtr quota_error_info),
              (override));

 private:
  mojo::Receiver<blink::mojom::AIManagerCreateSummarizerClient> receiver_{this};
};

blink::mojom::AISummarizerCreateOptionsPtr GetDefaultOptions() {
  return blink::mojom::AISummarizerCreateOptions::New(
      kSharedContextString, blink::mojom::AISummarizerType::kTLDR,
      blink::mojom::AISummarizerFormat::kPlainText,
      blink::mojom::AISummarizerLength::kMedium,
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

  mojo::Remote<blink::mojom::AISummarizer> GetAISummarizerRemote() {
    mojo::Remote<blink::mojom::AISummarizer> summarizer_remote;

    MockCreateSummarizerClient mock_create_summarizer_client;
    base::RunLoop run_loop;
    EXPECT_CALL(mock_create_summarizer_client, OnResult(_))
        .WillOnce(
            [&](mojo::PendingRemote<::blink::mojom::AISummarizer> summarizer) {
              EXPECT_TRUE(summarizer);
              summarizer_remote = mojo::Remote<blink::mojom::AISummarizer>(
                  std::move(summarizer));
              run_loop.Quit();
            });

    mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
    ai_manager->CreateSummarizer(
        mock_create_summarizer_client.BindNewPipeAndPassRemote(),
        GetDefaultOptions());
    run_loop.Run();

    return summarizer_remote;
  }

  void RunSimpleSummarizeTest(blink::mojom::AISummarizerType type,
                              blink::mojom::AISummarizerFormat format,
                              blink::mojom::AISummarizerLength length) {
    fake_broker_->settings().set_execute_result({"Result text"});

    const auto options = blink::mojom::AISummarizerCreateOptions::New(
        kSharedContextString, type, format, length,
        /*expected_input_languages=*/std::vector<AILanguageCodePtr>(),
        /*expected_context_languages=*/std::vector<AILanguageCodePtr>(),
        /*output_language=*/AILanguageCode::New(""));
    mojo::Remote<blink::mojom::AISummarizer> summarizer_remote;
    {
      MockCreateSummarizerClient mock_create_summarizer_client;
      base::RunLoop run_loop;
      EXPECT_CALL(mock_create_summarizer_client, OnResult(_))
          .WillOnce([&](mojo::PendingRemote<::blink::mojom::AISummarizer>
                            Summarizer) {
            EXPECT_TRUE(Summarizer);
            summarizer_remote =
                mojo::Remote<blink::mojom::AISummarizer>(std::move(Summarizer));
            run_loop.Quit();
          });

      mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
      ai_manager->CreateSummarizer(
          mock_create_summarizer_client.BindNewPipeAndPassRemote(),
          options.Clone());
      run_loop.Run();
    }

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
};

TEST(AISummarizerStandaloneTest, CombineContexts) {
  EXPECT_EQ("", AISummarizer::CombineContexts("", ""));
  EXPECT_EQ("a\n", AISummarizer::CombineContexts("a", ""));
  EXPECT_EQ("b\n", AISummarizer::CombineContexts("", "b"));
  EXPECT_EQ("a b\n", AISummarizer::CombineContexts("a", "b"));
}

TEST_F(AISummarizerTest, CanCreateDefaultOptions) {
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              GetOnDeviceModelEligibilityAsync(_, _, _))
      .WillOnce([](auto feature, auto capabilities, auto callback) {
        std::move(callback).Run(
            optimization_guide::OnDeviceModelEligibilityReason::kSuccess);
      });
  base::MockCallback<AIManager::CanCreateSummarizerCallback> callback;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::kAvailable));
  GetAIManagerInterface()->CanCreateSummarizer(GetDefaultOptions(),
                                               callback.Get());
}

TEST_F(AISummarizerTest, CanCreateIsLanguagesSupported) {
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
  base::MockCallback<AIManager::CanCreateSummarizerCallback> callback;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::kAvailable));
  GetAIManagerInterface()->CanCreateSummarizer(std::move(options),
                                               callback.Get());
}

TEST_F(AISummarizerTest, CanCreateUnIsLanguagesSupported) {
  auto options = GetDefaultOptions();
  options->output_language = AILanguageCode::New("es-ES");
  options->expected_input_languages =
      AITestUtils::ToMojoLanguageCodes({"en", "fr", "ja"});
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

  MockCreateSummarizerClient mock_create_summarizer_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_summarizer_client, OnError(_, _))
      .WillOnce([&](blink::mojom::AIManagerCreateClientError error,
                    blink::mojom::QuotaErrorInfoPtr quota_error_info) {
        ASSERT_EQ(
            error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
        run_loop.Quit();
      });

  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateSummarizer(
      mock_create_summarizer_client.BindNewPipeAndPassRemote(),
      GetDefaultOptions());
  run_loop.Run();
}

TEST_F(AISummarizerTest, CanCreateWaitsForEligibility) {
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
  GetAIManagerInterface()->CanCreateSummarizer(GetDefaultOptions(),
                                               result_future.GetCallback());
  // Session should not be ready until eligibility callback has run.
  EXPECT_FALSE(result_future.IsReady());
  eligibility_future.Take().Run(
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);
  EXPECT_EQ(result_future.Get(),
            blink::mojom::ModelAvailabilityCheckResult::kAvailable);
}

TEST_F(AISummarizerTest, CanCreateUnavailableWhenAdaptationNotAvailable) {
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              GetOnDeviceModelEligibilityAsync(_, _, _))
      .WillOnce([&](auto feature, auto capabilities, auto callback) {
        std::move(callback).Run(
            optimization_guide::OnDeviceModelEligibilityReason::
                kModelAdaptationNotAvailable);
      });

  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult>
      result_future;
  GetAIManagerInterface()->CanCreateSummarizer(GetDefaultOptions(),
                                               result_future.GetCallback());
  EXPECT_EQ(result_future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                                     kUnavailableModelAdaptationNotAvailable);
}

TEST_F(AISummarizerTest, CreateSummarizerUnableToCalculateTokenSize) {
  // Incorrect `request_base_name` cause session to fail constructing input
  // string and checking token size.
  auto config = CreateConfig();
  auto& input_config = *config.mutable_input_config();
  input_config.set_request_base_name("InvalidRequestBaseName");

  optimization_guide::FakeAdaptationAsset fake_asset({.config = config});
  fake_broker_->UpdateModelAdaptation(fake_asset);

  MockCreateSummarizerClient mock_create_summarizer_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_summarizer_client, OnError(_, _))
      .WillOnce([&](blink::mojom::AIManagerCreateClientError error,
                    blink::mojom::QuotaErrorInfoPtr quota_error_info) {
        ASSERT_EQ(error, blink::mojom::AIManagerCreateClientError::
                             kUnableToCalculateTokenSize);
        run_loop.Quit();
      });

  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateSummarizer(
      mock_create_summarizer_client.BindNewPipeAndPassRemote(),
      GetDefaultOptions());
  run_loop.Run();
}

TEST_F(AISummarizerTest, CreateSummarizerContextLimitExceededError) {
  fake_broker_->settings().set_size_in_tokens(
      blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);

  MockCreateSummarizerClient mock_create_summarizer_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_summarizer_client, OnError(_, _))
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
  ai_manager->CreateSummarizer(
      mock_create_summarizer_client.BindNewPipeAndPassRemote(),
      GetDefaultOptions());
  run_loop.Run();
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
  auto summarizer_remote = GetAISummarizerRemote();

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
      /*expected_input_languages=*/std::vector<AILanguageCodePtr>(),
      /*expected_context_languages=*/std::vector<AILanguageCodePtr>(),
      /*output_language=*/AILanguageCode::New(""));
  mojo::Remote<blink::mojom::AISummarizer> summarizer_remote;
  {
    MockCreateSummarizerClient mock_create_summarizer_client;
    base::RunLoop run_loop;
    EXPECT_CALL(mock_create_summarizer_client, OnResult(_))
        .WillOnce(
            [&](mojo::PendingRemote<::blink::mojom::AISummarizer> Summarizer) {
              EXPECT_TRUE(Summarizer);
              summarizer_remote = mojo::Remote<blink::mojom::AISummarizer>(
                  std::move(Summarizer));
              run_loop.Quit();
            });

    mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
    ai_manager->CreateSummarizer(
        mock_create_summarizer_client.BindNewPipeAndPassRemote(),
        options.Clone());
    run_loop.Run();
  }

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
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure);

  summarizer_remote = GetAISummarizerRemote();
  EXPECT_THAT(Summarize(*summarizer_remote, kInputString, kContextString),
              ElementsAre("hi"));
}

TEST_F(AISummarizerTest, CrashRecoveryMeasureInputUsage) {
  auto summarizer_remote = GetAISummarizerRemote();
  fake_broker_->CrashService();

  base::test::TestFuture<std::optional<uint32_t>> measure_future;
  summarizer_remote->MeasureUsage(kInputString, kContextString,
                                  measure_future.GetCallback());

  std::string context =
      AISummarizer::CombineContexts(kSharedContextString, kContextString);
  EXPECT_EQ(measure_future.Get(),
            std::string(kInputString).size() + context.size());
}

}  // namespace

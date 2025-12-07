// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_proofreader.h"

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
#include "components/optimization_guide/proto/features/proofreader_api.pb.h"
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
using ::optimization_guide::proto::ProofreaderApiRequest;
using ::optimization_guide::proto::ProofreaderApiResponse;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

constexpr char kInputString[] = "input string";
constexpr char kInputStringWithError[] = "`input` string";
constexpr char kCorrectedInputWithCorrection[] = "`Input` string.";
constexpr char kCorrectionInstruction[] = "From `input` to `Input`";

class MockCreateProofreaderClient
    : public blink::mojom::AIManagerCreateProofreaderClient {
 public:
  MockCreateProofreaderClient() = default;
  ~MockCreateProofreaderClient() override = default;
  MockCreateProofreaderClient(const MockCreateProofreaderClient&) = delete;
  MockCreateProofreaderClient& operator=(const MockCreateProofreaderClient&) =
      delete;

  mojo::PendingRemote<blink::mojom::AIManagerCreateProofreaderClient>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void,
              OnResult,
              (mojo::PendingRemote<::blink::mojom::AIProofreader> proofreader),
              (override));
  MOCK_METHOD(void,
              OnError,
              (blink::mojom::AIManagerCreateClientError error,
               blink::mojom::QuotaErrorInfoPtr quota_error_info),
              (override));

 private:
  mojo::Receiver<blink::mojom::AIManagerCreateProofreaderClient> receiver_{
      this};
};

blink::mojom::AIProofreaderCreateOptionsPtr GetDefaultOptions() {
  return blink::mojom::AIProofreaderCreateOptions::New(
      /*include_correction_types=*/false,
      /*include_correction_explanations=*/false,
      /*correction_explanation_language=*/AILanguageCode::New(""),
      /*expected_input_languages=*/std::vector<AILanguageCodePtr>());
}

optimization_guide::proto::FeatureTextSafetyConfiguration CreateSafetyConfig() {
  optimization_guide::proto::FeatureTextSafetyConfiguration safety_config;
  safety_config.set_feature(
      optimization_guide::proto::MODEL_EXECUTION_FEATURE_PROOFREADER_API);
  safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());

  {
    auto* check = safety_config.add_request_check();
    check->mutable_input_template()->Add(FieldSubstitution(
        "%s", ProtoField({ProofreaderApiRequest::kTextFieldNumber})));
  }
  {
    auto* check = safety_config.add_request_check();
    check->mutable_input_template()->Add(FieldSubstitution(
        "%s", ProtoField({ProofreaderApiRequest::kCorrectedTextFieldNumber})));
  }
  {
    auto* check = safety_config.add_request_check();
    check->mutable_input_template()->Add(FieldSubstitution(
        "%s", ProtoField({ProofreaderApiRequest::kCorrectionFieldNumber})));
  }

  return safety_config;
}

class AIProofreaderTest : public AITestUtils::AITestBase {
 protected:
  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig CreateConfig()
      override {
    optimization_guide::proto::OnDeviceModelExecutionFeatureConfig config;
    config.set_can_skip_text_safety(true);
    config.set_feature(optimization_guide::proto::ModelExecutionFeature::
                           MODEL_EXECUTION_FEATURE_PROOFREADER_API);

    auto& input_config = *config.mutable_input_config();
    input_config.set_request_base_name(ProofreaderApiRequest().GetTypeName());

    *input_config.add_execute_substitutions() = FieldSubstitution(
        "%s", ProtoField({ProofreaderApiRequest::kTextFieldNumber}));
    *input_config.add_execute_substitutions() = FieldSubstitution(
        "%s", ProtoField({ProofreaderApiRequest::kCorrectedTextFieldNumber}));
    *input_config.add_execute_substitutions() = FieldSubstitution(
        "%s", ProtoField({ProofreaderApiRequest::kCorrectionFieldNumber}));

    auto& output_config = *config.mutable_output_config();
    output_config.set_proto_type(ProofreaderApiResponse().GetTypeName());
    *output_config.mutable_proto_field() = StringValueField();

    return config;
  }

  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig
  CreateSafeConfig() {
    auto config = CreateConfig();
    config.set_can_skip_text_safety(false);
    return config;
  }

  mojo::Remote<blink::mojom::AIProofreader> GetAIProofreaderRemote() {
    mojo::Remote<blink::mojom::AIProofreader> proofreader_remote;

    MockCreateProofreaderClient mock_create_proofreader_client;
    base::RunLoop run_loop;
    EXPECT_CALL(mock_create_proofreader_client, OnResult(_))
        .WillOnce([&](mojo::PendingRemote<::blink::mojom::AIProofreader>
                          proofreader) {
          EXPECT_TRUE(proofreader);
          proofreader_remote =
              mojo::Remote<blink::mojom::AIProofreader>(std::move(proofreader));
          run_loop.Quit();
        });

    mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
    ai_manager->CreateProofreader(
        mock_create_proofreader_client.BindNewPipeAndPassRemote(),
        GetDefaultOptions());
    run_loop.Run();

    return proofreader_remote;
  }

  void RunSimpleProofreadTest(bool include_correction_types,
                              bool include_correction_explanations) {
    fake_broker_->settings().set_execute_result({"Result text"});

    const auto options = blink::mojom::AIProofreaderCreateOptions::New(
        include_correction_types, include_correction_explanations,
        /*correction_explanation_language=*/AILanguageCode::New(""),
        /*expected_input_languages=*/std::vector<AILanguageCodePtr>());

    mojo::Remote<blink::mojom::AIProofreader> proofreader_remote;
    {
      MockCreateProofreaderClient mock_create_proofreader_client;
      base::RunLoop run_loop;
      EXPECT_CALL(mock_create_proofreader_client, OnResult(_))
          .WillOnce([&](mojo::PendingRemote<::blink::mojom::AIProofreader>
                            proofreader) {
            EXPECT_TRUE(proofreader);
            proofreader_remote = mojo::Remote<blink::mojom::AIProofreader>(
                std::move(proofreader));
            run_loop.Quit();
          });

      mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
      ai_manager->CreateProofreader(
          mock_create_proofreader_client.BindNewPipeAndPassRemote(),
          options.Clone());
      run_loop.Run();
    }

    EXPECT_THAT(Proofread(*proofreader_remote, kInputString),
                ElementsAreArray({"Result text"}));
  }

  std::vector<std::string> Proofread(blink::mojom::AIProofreader& proofreader,
                                     const std::string& input) {
    AITestUtils::TestStreamingResponder responder;
    proofreader.Proofread(kInputString, responder.BindRemote());
    EXPECT_TRUE(responder.WaitForCompletion());
    // Return Proofreader's response without the final empty string chunk.
    return responder.responses_without_last();
  }
};

TEST_F(AIProofreaderTest, CreateProofreaderNoService) {
  SetupNullOptimizationGuideKeyedService();

  MockCreateProofreaderClient mock_create_proofreader_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_proofreader_client, OnError(_, _))
      .WillOnce([&](blink::mojom::AIManagerCreateClientError error,
                    blink::mojom::QuotaErrorInfoPtr quota_error_info) {
        ASSERT_EQ(
            error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
        run_loop.Quit();
      });

  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateProofreader(
      mock_create_proofreader_client.BindNewPipeAndPassRemote(),
      GetDefaultOptions());
  run_loop.Run();
}

TEST_F(AIProofreaderTest, CanCreateWaitsForEligibility) {
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
  GetAIManagerInterface()->CanCreateProofreader(GetDefaultOptions(),
                                                result_future.GetCallback());
  // Session should not be ready until eligibility callback has run.
  EXPECT_FALSE(result_future.IsReady());
  eligibility_future.Take().Run(
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);
  EXPECT_EQ(result_future.Get(),
            blink::mojom::ModelAvailabilityCheckResult::kAvailable);
}

TEST_F(AIProofreaderTest, CanCreateUnavailableWhenAdaptationNotAvailable) {
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              GetOnDeviceModelEligibilityAsync(_, _, _))
      .WillOnce([&](auto feature, auto capabilities, auto callback) {
        std::move(callback).Run(
            optimization_guide::OnDeviceModelEligibilityReason::
                kModelAdaptationNotAvailable);
      });

  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult>
      result_future;
  GetAIManagerInterface()->CanCreateProofreader(GetDefaultOptions(),
                                                result_future.GetCallback());
  EXPECT_EQ(result_future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                                     kUnavailableModelAdaptationNotAvailable);
}

TEST_F(AIProofreaderTest, CanCreateDefaultOptions) {
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              GetOnDeviceModelEligibilityAsync(_, _, _))
      .WillOnce([](auto feature, auto capabilities, auto callback) {
        std::move(callback).Run(
            optimization_guide::OnDeviceModelEligibilityReason::kSuccess);
      });
  base::MockCallback<AIManager::CanCreateProofreaderCallback> callback;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::kAvailable));
  GetAIManagerInterface()->CanCreateProofreader(GetDefaultOptions(),
                                                callback.Get());
}

TEST_F(AIProofreaderTest, CanCreateIsLanguagesSupported) {
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              GetOnDeviceModelEligibilityAsync(_, _, _))
      .WillOnce([](auto feature, auto capabilities, auto callback) {
        std::move(callback).Run(
            optimization_guide::OnDeviceModelEligibilityReason::kSuccess);
      });
  auto options = GetDefaultOptions();
  options->correction_explanation_language = AILanguageCode::New("en");
  options->expected_input_languages =
      AITestUtils::ToMojoLanguageCodes({"en-US", ""});
  base::MockCallback<AIManager::CanCreateProofreaderCallback> callback;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::kAvailable));
  GetAIManagerInterface()->CanCreateProofreader(std::move(options),
                                                callback.Get());
}

TEST_F(AIProofreaderTest, CanCreateUnIsLanguagesSupported) {
  auto options = GetDefaultOptions();
  options->correction_explanation_language = AILanguageCode::New("es-ES");
  options->expected_input_languages =
      AITestUtils::ToMojoLanguageCodes({"en", "fr", "ja"});
  base::MockCallback<AIManager::CanCreateProofreaderCallback> callback;
  EXPECT_CALL(callback, Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage));
  GetAIManagerInterface()->CanCreateProofreader(std::move(options),
                                                callback.Get());
}

TEST_F(AIProofreaderTest, ProofreadDefault) {
  RunSimpleProofreadTest(false, false);
}

TEST_F(AIProofreaderTest, ProofreadWithOptions) {
  bool types[]{false, true};
  bool explanations[]{false, true};
  for (const auto& include_correction_types : types) {
    for (const auto& include_correction_explanations : explanations) {
      SCOPED_TRACE(testing::Message() << include_correction_types << " "
                                      << include_correction_explanations);
      RunSimpleProofreadTest(include_correction_types,
                             include_correction_explanations);
    }
  }
}

TEST_F(AIProofreaderTest, InputLimitExceededError) {
  auto proofreader_remote = GetAIProofreaderRemote();

  fake_broker_->settings().set_size_in_tokens(
      blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);

  AITestUtils::TestStreamingResponder responder;
  proofreader_remote->Proofread(kInputString, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorInputTooLarge);
  ASSERT_EQ(responder.quota_error_info().requested,
            blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);
  ASSERT_EQ(responder.quota_error_info().quota,
            blink::mojom::kWritingAssistanceMaxInputTokenSize);
}

TEST_F(AIProofreaderTest, ProofreadMultipleResponse) {
  auto proofreader_remote = GetAIProofreaderRemote();

  std::vector<std::string> result = {"Result ", "text"};
  fake_broker_->settings().set_execute_result(result);
  EXPECT_THAT(Proofread(*proofreader_remote, kInputString),
              ElementsAreArray(result));
}

TEST_F(AIProofreaderTest, MultipleProofread) {
  auto proofreader_remote = GetAIProofreaderRemote();

  std::vector<std::string> result = {"Result ", "text"};
  fake_broker_->settings().set_execute_result(result);
  EXPECT_THAT(Proofread(*proofreader_remote, kInputString),
              ElementsAreArray(result));

  std::vector<std::string> result2 = {"Result ", "text ", "2"};
  fake_broker_->settings().set_execute_result(result2);
  EXPECT_THAT(Proofread(*proofreader_remote, "input string 2"),
              ElementsAreArray(result2));
}

TEST_F(AIProofreaderTest, GetCorretionTypeDefault) {
  fake_broker_->settings().set_execute_result({"Correction type"});

  const auto options = blink::mojom::AIProofreaderCreateOptions::New(
      /*include_correction_types=*/true,
      /*include_correction_explanations=*/false,
      /*correction_explanation_language=*/AILanguageCode::New(""),
      /*expected_input_languages=*/std::vector<AILanguageCodePtr>());
  mojo::Remote<blink::mojom::AIProofreader> proofreader_remote;
  {
    MockCreateProofreaderClient mock_create_proofreader_client;
    base::RunLoop run_loop;
    EXPECT_CALL(mock_create_proofreader_client, OnResult(_))
        .WillOnce([&](mojo::PendingRemote<::blink::mojom::AIProofreader>
                          proofreader) {
          EXPECT_TRUE(proofreader);
          proofreader_remote =
              mojo::Remote<blink::mojom::AIProofreader>(std::move(proofreader));
          run_loop.Quit();
        });

    mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
    ai_manager->CreateProofreader(
        mock_create_proofreader_client.BindNewPipeAndPassRemote(),
        options.Clone());
    run_loop.Run();
  }

  AITestUtils::TestStreamingResponder responder;
  proofreader_remote->GetCorrectionType(
      kInputStringWithError, kCorrectedInputWithCorrection,
      kCorrectionInstruction, responder.BindRemote());
  EXPECT_TRUE(responder.WaitForCompletion());
  EXPECT_THAT(responder.responses_without_last(),
              ElementsAreArray({"Correction type"}));
}

TEST_F(AIProofreaderTest, Priority) {
  fake_broker_->settings().set_execute_result({"hi"});
  auto proofreader_remote = GetAIProofreaderRemote();

  EXPECT_THAT(Proofread(*proofreader_remote, kInputString), ElementsAre("hi"));

  main_rfh()->GetRenderWidgetHost()->GetView()->Hide();
  EXPECT_THAT(Proofread(*proofreader_remote, kInputString),
              ElementsAre("Priority: background", "hi"));

  main_rfh()->GetRenderWidgetHost()->GetView()->Show();
  EXPECT_THAT(Proofread(*proofreader_remote, kInputString), ElementsAre("hi"));
}

TEST_F(AIProofreaderTest, TextSafetyInput) {
  optimization_guide::FakeAdaptationAsset fake_asset(
      {.config = CreateSafeConfig()});
  fake_broker_->UpdateModelAdaptation(fake_asset);
  optimization_guide::FakeSafetyModelAsset safety_asset(CreateSafetyConfig());
  fake_broker_->UpdateSafetyModel(safety_asset);

  fake_broker_->settings().set_execute_result({"hi"});
  auto proofreader_remote = GetAIProofreaderRemote();
  EXPECT_THAT(Proofread(*proofreader_remote, kInputString), ElementsAre("hi"));

  AITestUtils::TestStreamingResponder responder;
  proofreader_remote->Proofread("unsafe", responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
}

TEST_F(AIProofreaderTest, TextSafetyOutput) {
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
  auto proofreader_remote = GetAIProofreaderRemote();
  AITestUtils::TestStreamingResponder responder;
  proofreader_remote->Proofread(kInputString, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
  EXPECT_TRUE(responder.responses().empty());
}

TEST_F(AIProofreaderTest, TextSafetyOutputPartial) {
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
  auto proofreader_remote = GetAIProofreaderRemote();
  AITestUtils::TestStreamingResponder responder;
  proofreader_remote->Proofread(kInputString, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
  // Partial checks should still allow some output to stream.
  EXPECT_THAT(responder.responses(), ElementsAre("abc", "de", "fg"));
}

TEST_F(AIProofreaderTest, ServiceCrash) {
  fake_broker_->settings().set_execute_result({"hi"});

  auto proofreader_remote = GetAIProofreaderRemote();
  AITestUtils::TestStreamingResponder responder;
  proofreader_remote->Proofread(kInputString, responder.BindRemote());
  fake_broker_->CrashService();

  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure);

  proofreader_remote = GetAIProofreaderRemote();
  EXPECT_THAT(Proofread(*proofreader_remote, kInputString), ElementsAre("hi"));
}

}  // namespace

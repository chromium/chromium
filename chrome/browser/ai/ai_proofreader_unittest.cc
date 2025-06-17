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
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/proofreader_api.pb.h"
#include "content/public/browser/render_widget_host_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace {

using ::base::test::EqualsProto;
using ::blink::mojom::AILanguageCode;
using ::blink::mojom::AILanguageCodePtr;
using ::testing::_;

constexpr char kInputString[] = "input string";

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

optimization_guide::OptimizationGuideModelStreamingExecutionResult
CreateExecutionResult(std::string_view output, bool is_complete) {
  optimization_guide::proto::ProofreaderApiResponse response;
  *response.mutable_output() = output;
  return optimization_guide::OptimizationGuideModelStreamingExecutionResult(
      optimization_guide::StreamingResponse{
          .response = optimization_guide::AnyWrapProto(response),
          .is_complete = is_complete,
      },
      /*provided_by_on_device=*/true);
}

optimization_guide::OptimizationGuideModelStreamingExecutionResult
CreateExecutionErrorResult(
    optimization_guide::OptimizationGuideModelExecutionError error) {
  return optimization_guide::OptimizationGuideModelStreamingExecutionResult(
      base::unexpected(error),
      /*provided_by_on_device=*/true);
}

blink::mojom::AIProofreaderCreateOptionsPtr GetDefaultOptions() {
  return blink::mojom::AIProofreaderCreateOptions::New(
      /*include_correction_types=*/false,
      /*include_correction_explanations=*/false,
      /*correction_explanation_language=*/AILanguageCode::New(""),
      /*expected_input_languages=*/std::vector<AILanguageCodePtr>());
}

// Get a request proto matching that expected for ExecuteModel() calls.
optimization_guide::proto::ProofreaderApiRequest GetExecuteRequest(
    std::string_view proofread_text = kInputString) {
  optimization_guide::proto::ProofreaderApiRequest request;
  request.set_allocated_options(
      AIProofreader::ToProtoOptions(GetDefaultOptions()).release());
  request.set_text(proofread_text);
  return request;
}

class AIProofreaderTest : public AITestUtils::AITestBase {
 protected:
  mojo::Remote<blink::mojom::AIProofreader> GetAIProofreaderRemote() {
    mojo::Remote<blink::mojom::AIProofreader> proofreader_remote;

    MockCreateProofreaderClient mock_create_proofreader_client;
    base::RunLoop run_loop;
    EXPECT_CALL(mock_create_proofreader_client, OnResult(_))
        .WillOnce(testing::Invoke(
            [&](mojo::PendingRemote<::blink::mojom::AIProofreader>
                    proofreader) {
              EXPECT_TRUE(proofreader);
              proofreader_remote = mojo::Remote<blink::mojom::AIProofreader>(
                  std::move(proofreader));
              run_loop.Quit();
            }));

    mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
    ai_manager->CreateProofreader(
        mock_create_proofreader_client.BindNewPipeAndPassRemote(),
        GetDefaultOptions());
    run_loop.Run();

    return proofreader_remote;
  }

  void RunSimpleProofreadTest(bool include_correction_types,
                              bool include_correction_explanations) {
    auto expected = GetExecuteRequest();
    const auto options = blink::mojom::AIProofreaderCreateOptions::New(
        include_correction_types, include_correction_explanations,
        /*correction_explanation_language=*/AILanguageCode::New(""),
        /*expected_input_languages=*/std::vector<AILanguageCodePtr>());
    expected.set_allocated_options(
        AIProofreader::ToProtoOptions(options).release());
    EXPECT_CALL(session_, ExecuteModel(_, _))
        .WillOnce(testing::Invoke(
            [&](const google::protobuf::MessageLite& request,
                optimization_guide::
                    OptimizationGuideModelExecutionResultStreamingCallback
                        callback) {
              EXPECT_THAT(request, EqualsProto(expected));
              callback.Run(CreateExecutionResult("Result text",
                                                 /*is_complete=*/true));
            }));

    mojo::Remote<blink::mojom::AIProofreader> proofreader_remote;
    {
      MockCreateProofreaderClient mock_create_proofreader_client;
      base::RunLoop run_loop;
      EXPECT_CALL(mock_create_proofreader_client, OnResult(_))
          .WillOnce(testing::Invoke(
              [&](mojo::PendingRemote<::blink::mojom::AIProofreader>
                      proofreader) {
                EXPECT_TRUE(proofreader);
                proofreader_remote = mojo::Remote<blink::mojom::AIProofreader>(
                    std::move(proofreader));
                run_loop.Quit();
              }));

      mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
      ai_manager->CreateProofreader(
          mock_create_proofreader_client.BindNewPipeAndPassRemote(),
          options.Clone());
      run_loop.Run();
    }
    AITestUtils::MockModelStreamingResponder mock_responder;

    base::RunLoop run_loop;
    EXPECT_CALL(mock_responder, OnStreaming(_))
        .WillOnce(testing::Invoke([&](const std::string& text) {
          EXPECT_THAT(text, "Result text");
        }));

    EXPECT_CALL(mock_responder, OnCompletion(_))
        .WillOnce(testing::Invoke(
            [&](blink::mojom::ModelExecutionContextInfoPtr context_info) {
              run_loop.Quit();
            }));

    proofreader_remote->Proofread(kInputString,
                                  mock_responder.BindNewPipeAndPassRemote());
    run_loop.Run();
  }
};

TEST_F(AIProofreaderTest, CreateProofreaderNoService) {
  SetupNullOptimizationGuideKeyedService();

  MockCreateProofreaderClient mock_create_proofreader_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_proofreader_client, OnError(_, _))
      .WillOnce(testing::Invoke(
          [&](blink::mojom::AIManagerCreateClientError error,
              blink::mojom::QuotaErrorInfoPtr quota_error_info) {
            ASSERT_EQ(error, blink::mojom::AIManagerCreateClientError::
                                 kUnableToCreateSession);
            run_loop.Quit();
          }));

  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateProofreader(
      mock_create_proofreader_client.BindNewPipeAndPassRemote(),
      GetDefaultOptions());
  run_loop.Run();
}

TEST_F(AIProofreaderTest, CreateProofreaderModelNotEligible) {
  SetupMockOptimizationGuideKeyedService();
  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) { return nullptr; }));
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              GetOnDeviceModelEligibilityAsync(_, _, _))
      .WillOnce([](auto feature, auto capabilities, auto callback) {
        std::move(callback).Run(
            optimization_guide::OnDeviceModelEligibilityReason::
                kModelNotEligible);
      });

  MockCreateProofreaderClient mock_create_proofreader_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_proofreader_client, OnError(_, _))
      .WillOnce(testing::Invoke(
          [&](blink::mojom::AIManagerCreateClientError error,
              blink::mojom::QuotaErrorInfoPtr quota_error_info) {
            ASSERT_EQ(error, blink::mojom::AIManagerCreateClientError::
                                 kUnableToCreateSession);
            run_loop.Quit();
          }));

  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateProofreader(
      mock_create_proofreader_client.BindNewPipeAndPassRemote(),
      GetDefaultOptions());
  run_loop.Run();
}

TEST_F(AIProofreaderTest,
       CreateProofreaderAbortAfterConfigNotAvailableForFeature) {
  SetupMockOptimizationGuideKeyedService();

  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) { return nullptr; }));

  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              GetOnDeviceModelEligibilityAsync(_, _, _))
      .WillOnce([](auto feature, auto capabilities, auto callback) {
        // Returning kConfigNotAvailableForFeature should trigger retry.
        std::move(callback).Run(
            optimization_guide::OnDeviceModelEligibilityReason::
                kConfigNotAvailableForFeature);
      });

  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  base::RunLoop run_loop_for_remove_observer;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              RemoveOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            EXPECT_EQ(availability_observer, observer);
            run_loop_for_remove_observer.Quit();
          }));

  auto mock_create_proofreader_client =
      std::make_unique<MockCreateProofreaderClient>();
  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateProofreader(
      mock_create_proofreader_client->BindNewPipeAndPassRemote(),
      GetDefaultOptions());

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  // Reset `mock_create_proofreader_client` to abort the task of
  // CreateProofreader().
  mock_create_proofreader_client.reset();

  // RemoveOnDeviceModelAvailabilityChangeObserver should be called.
  run_loop_for_remove_observer.Run();
}

TEST_F(AIProofreaderTest, CanCreateDefaultOptions) {
  SetupMockOptimizationGuideKeyedService();
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
  SetupMockOptimizationGuideKeyedService();
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
  SetupMockOptimizationGuideKeyedService();
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
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
  RunSimpleProofreadTest(false, false);
}

TEST_F(AIProofreaderTest, ProofreadWithOptions) {
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
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
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
  auto proofreader_remote = GetAIProofreaderRemote();

  EXPECT_CALL(session_, GetExecutionInputSizeInTokens(_, _))
      .WillOnce(testing::Invoke(
          [](optimization_guide::MultimodalMessageReadView request_metadata,
             optimization_guide::OptimizationGuideModelSizeInTokenCallback
                 callback) {
            std::move(callback).Run(
                blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);
          }));
  AITestUtils::MockModelStreamingResponder mock_responder;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_responder, OnError(_, _))
      .WillOnce(testing::Invoke(
          [&](blink::mojom::ModelStreamingResponseStatus status,
              blink::mojom::QuotaErrorInfoPtr quota_error_info) {
            EXPECT_EQ(status, blink::mojom::ModelStreamingResponseStatus::
                                  kErrorInputTooLarge);
            ASSERT_TRUE(quota_error_info);
            ASSERT_EQ(quota_error_info->requested,
                      blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);
            ASSERT_EQ(quota_error_info->quota,
                      blink::mojom::kWritingAssistanceMaxInputTokenSize);
            run_loop.Quit();
          }));

  proofreader_remote->Proofread(kInputString,
                                mock_responder.BindNewPipeAndPassRemote());
  run_loop.Run();
}

TEST_F(AIProofreaderTest, ModelExecutionError) {
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(testing::Invoke(
          [](const google::protobuf::MessageLite& request,
             optimization_guide::
                 OptimizationGuideModelExecutionResultStreamingCallback
                     callback) {
            EXPECT_THAT(request, EqualsProto(GetExecuteRequest()));
            callback.Run(CreateExecutionErrorResult(
                optimization_guide::OptimizationGuideModelExecutionError::
                    FromModelExecutionError(
                        optimization_guide::
                            OptimizationGuideModelExecutionError::
                                ModelExecutionError::kPermissionDenied)));
          }));

  auto proofreader_remote = GetAIProofreaderRemote();
  AITestUtils::MockModelStreamingResponder mock_responder;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_responder, OnError(_, _))
      .WillOnce(testing::Invoke(
          [&](blink::mojom::ModelStreamingResponseStatus status,
              blink::mojom::QuotaErrorInfoPtr quota_error_info) {
            EXPECT_EQ(status, blink::mojom::ModelStreamingResponseStatus::
                                  kErrorPermissionDenied);
            run_loop.Quit();
          }));

  proofreader_remote->Proofread(kInputString,
                                mock_responder.BindNewPipeAndPassRemote());
  run_loop.Run();
}

TEST_F(AIProofreaderTest, ProofreadMultipleResponse) {
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(testing::Invoke(
          [](const google::protobuf::MessageLite& request,
             optimization_guide::
                 OptimizationGuideModelExecutionResultStreamingCallback
                     callback) {
            EXPECT_THAT(request, EqualsProto(GetExecuteRequest()));
            callback.Run(
                CreateExecutionResult("Result ", /*is_complete=*/false));
            callback.Run(CreateExecutionResult("text",
                                               /*is_complete=*/true));
          }));

  auto proofreader_remote = GetAIProofreaderRemote();
  AITestUtils::MockModelStreamingResponder mock_responder;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_responder, OnStreaming(_))
      .WillOnce(testing::Invoke(
          [&](const std::string& text) { EXPECT_THAT(text, "Result "); }))
      .WillOnce(testing::Invoke(
          [&](const std::string& text) { EXPECT_THAT(text, "text"); }));

  EXPECT_CALL(mock_responder, OnCompletion(_))
      .WillOnce(testing::Invoke(
          [&](blink::mojom::ModelExecutionContextInfoPtr context_info) {
            run_loop.Quit();
          }));

  proofreader_remote->Proofread(kInputString,
                                mock_responder.BindNewPipeAndPassRemote());
  run_loop.Run();
}

TEST_F(AIProofreaderTest, MultipleProofread) {
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(testing::Invoke(
          [](const google::protobuf::MessageLite& request,
             optimization_guide::
                 OptimizationGuideModelExecutionResultStreamingCallback
                     callback) {
            EXPECT_THAT(request, EqualsProto(GetExecuteRequest()));
            callback.Run(CreateExecutionResult("Result text",
                                               /*is_complete=*/true));
          }))
      .WillOnce(testing::Invoke(
          [](const google::protobuf::MessageLite& request,
             optimization_guide::
                 OptimizationGuideModelExecutionResultStreamingCallback
                     callback) {
            auto expect = GetExecuteRequest("input string 2");
            EXPECT_THAT(request, EqualsProto(expect));
            callback.Run(CreateExecutionResult("Result text 2",
                                               /*is_complete=*/true));
          }));

  auto proofreader_remote = GetAIProofreaderRemote();
  {
    AITestUtils::MockModelStreamingResponder mock_responder;
    base::RunLoop run_loop;
    EXPECT_CALL(mock_responder, OnStreaming(_))
        .WillOnce(testing::Invoke([&](const std::string& text) {
          EXPECT_THAT(text, "Result text");
        }));

    EXPECT_CALL(mock_responder, OnCompletion(_))
        .WillOnce(testing::Invoke(
            [&](blink::mojom::ModelExecutionContextInfoPtr context_info) {
              run_loop.Quit();
            }));

    proofreader_remote->Proofread(kInputString,
                                  mock_responder.BindNewPipeAndPassRemote());
    run_loop.Run();
  }
  {
    AITestUtils::MockModelStreamingResponder mock_responder;
    base::RunLoop run_loop;
    EXPECT_CALL(mock_responder, OnStreaming(_))
        .WillOnce(testing::Invoke([&](const std::string& text) {
          EXPECT_THAT(text, "Result text 2");
        }));

    EXPECT_CALL(mock_responder, OnCompletion(_))
        .WillOnce(testing::Invoke(
            [&](blink::mojom::ModelExecutionContextInfoPtr context_info) {
              run_loop.Quit();
            }));

    proofreader_remote->Proofread("input string 2",
                                  mock_responder.BindNewPipeAndPassRemote());
    run_loop.Run();
  }
}

TEST_F(AIProofreaderTest, ResponderDisconnected) {
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
  base::RunLoop run_loop_for_callback;
  optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
      streaming_callback;
  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(testing::Invoke(
          [&](const google::protobuf::MessageLite& request,
              optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            EXPECT_THAT(request, EqualsProto(GetExecuteRequest()));
            streaming_callback = std::move(callback);
            run_loop_for_callback.Quit();
          }));

  auto proofreader_remote = GetAIProofreaderRemote();
  std::unique_ptr<AITestUtils::MockModelStreamingResponder> mock_responder =
      std::make_unique<AITestUtils::MockModelStreamingResponder>();
  proofreader_remote->Proofread(kInputString,
                                mock_responder->BindNewPipeAndPassRemote());
  mock_responder.reset();
  // Call RunUntilIdle() to disconnect the ModelStreamingResponder mojo remote
  // interface in AIProofreader.
  task_environment()->RunUntilIdle();

  run_loop_for_callback.Run();
  ASSERT_TRUE(streaming_callback);
  streaming_callback.Run(CreateExecutionResult("Result text",
                                               /*is_complete=*/true));
  task_environment()->RunUntilIdle();
}

TEST_F(AIProofreaderTest, ProofreaderDisconnected) {
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
  base::RunLoop run_loop_for_callback;
  optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
      streaming_callback;
  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(testing::Invoke(
          [&](const google::protobuf::MessageLite& request,
              optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            EXPECT_THAT(request, EqualsProto(GetExecuteRequest()));
            streaming_callback = std::move(callback);
            run_loop_for_callback.Quit();
          }));

  auto proofreader_remote = GetAIProofreaderRemote();
  AITestUtils::MockModelStreamingResponder mock_responder;
  base::RunLoop run_loop_for_response;
  EXPECT_CALL(mock_responder, OnError(_, _))
      .WillOnce(testing::Invoke(
          [&](blink::mojom::ModelStreamingResponseStatus status,
              blink::mojom::QuotaErrorInfoPtr quota_error_info) {
            EXPECT_EQ(status, blink::mojom::ModelStreamingResponseStatus::
                                  kErrorSessionDestroyed);
            run_loop_for_response.Quit();
          }));

  proofreader_remote->Proofread(kInputString,
                                mock_responder.BindNewPipeAndPassRemote());

  run_loop_for_callback.Run();

  // Disconnect the proofreader handle.
  proofreader_remote.reset();

  // Call RunUntilIdle() to destroy AIProofreader.
  task_environment()->RunUntilIdle();

  ASSERT_TRUE(streaming_callback);
  streaming_callback.Run(CreateExecutionResult("Result text",
                                               /*is_complete=*/true));
  run_loop_for_response.Run();
}

}  // namespace

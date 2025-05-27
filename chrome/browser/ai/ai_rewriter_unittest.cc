// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_rewriter.h"

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
#include "components/optimization_guide/proto/features/writing_assistance_api.pb.h"
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

constexpr char kSharedContextString[] = "test shared context";
constexpr char kContextString[] = "test context";
constexpr char kInputString[] = "input string";

class MockCreateRewriterClient
    : public blink::mojom::AIManagerCreateRewriterClient {
 public:
  MockCreateRewriterClient() = default;
  ~MockCreateRewriterClient() override = default;
  MockCreateRewriterClient(const MockCreateRewriterClient&) = delete;
  MockCreateRewriterClient& operator=(const MockCreateRewriterClient&) = delete;

  mojo::PendingRemote<blink::mojom::AIManagerCreateRewriterClient>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void,
              OnResult,
              (mojo::PendingRemote<::blink::mojom::AIRewriter> rewriter),
              (override));
  MOCK_METHOD(void,
              OnError,
              (blink::mojom::AIManagerCreateClientError error,
               blink::mojom::QuotaErrorInfoPtr quota_error_info),
              (override));

 private:
  mojo::Receiver<blink::mojom::AIManagerCreateRewriterClient> receiver_{this};
};

optimization_guide::OptimizationGuideModelStreamingExecutionResult
CreateExecutionResult(std::string_view output, bool is_complete) {
  optimization_guide::proto::WritingAssistanceApiResponse response;
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

blink::mojom::AIRewriterCreateOptionsPtr GetDefaultOptions() {
  return blink::mojom::AIRewriterCreateOptions::New(
      kSharedContextString, blink::mojom::AIRewriterTone::kAsIs,
      blink::mojom::AIRewriterFormat::kAsIs,
      blink::mojom::AIRewriterLength::kAsIs,
      /*expected_input_languages=*/std::vector<AILanguageCodePtr>(),
      /*expected_context_languages=*/std::vector<AILanguageCodePtr>(),
      /*output_language=*/AILanguageCode::New(""));
}

// Get a request proto matching that expected for ExecuteModel() calls.
optimization_guide::proto::WritingAssistanceApiRequest GetExecuteRequest(
    std::string_view context_string = kContextString,
    std::string_view rewrite_text = kInputString) {
  optimization_guide::proto::WritingAssistanceApiRequest request;
  request.set_context(context_string);
  request.set_allocated_options(
      AIRewriter::ToProtoOptions(GetDefaultOptions()).release());
  request.set_rewrite_text(rewrite_text);
  request.set_shared_context(kSharedContextString);
  return request;
}

class AIRewriterTest : public AITestUtils::AITestBase {
 protected:
  mojo::Remote<blink::mojom::AIRewriter> GetAIRewriterRemote() {
    mojo::Remote<blink::mojom::AIRewriter> rewriter_remote;

    MockCreateRewriterClient mock_create_rewriter_client;
    base::RunLoop run_loop;
    EXPECT_CALL(mock_create_rewriter_client, OnResult(_))
        .WillOnce(testing::Invoke(
            [&](mojo::PendingRemote<::blink::mojom::AIRewriter> rewriter) {
              EXPECT_TRUE(rewriter);
              rewriter_remote =
                  mojo::Remote<blink::mojom::AIRewriter>(std::move(rewriter));
              run_loop.Quit();
            }));

    mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
    ai_manager->CreateRewriter(
        mock_create_rewriter_client.BindNewPipeAndPassRemote(),
        GetDefaultOptions());
    run_loop.Run();

    return rewriter_remote;
  }

  void RunSimpleRewriteTest(blink::mojom::AIRewriterTone tone,
                            blink::mojom::AIRewriterFormat format,
                            blink::mojom::AIRewriterLength length) {
    auto expected = GetExecuteRequest();
    const auto options = blink::mojom::AIRewriterCreateOptions::New(
        kSharedContextString, tone, format, length,
        /*expected_input_languages=*/std::vector<AILanguageCodePtr>(),
        /*expected_context_languages=*/std::vector<AILanguageCodePtr>(),
        /*output_language=*/AILanguageCode::New(""));
    expected.set_allocated_options(
        AIRewriter::ToProtoOptions(options).release());
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

    mojo::Remote<blink::mojom::AIRewriter> rewriter_remote;
    {
      MockCreateRewriterClient mock_create_rewriter_client;
      base::RunLoop run_loop;
      EXPECT_CALL(mock_create_rewriter_client, OnResult(_))
          .WillOnce(testing::Invoke(
              [&](mojo::PendingRemote<::blink::mojom::AIRewriter> rewriter) {
                EXPECT_TRUE(rewriter);
                rewriter_remote =
                    mojo::Remote<blink::mojom::AIRewriter>(std::move(rewriter));
                run_loop.Quit();
              }));

      mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
      ai_manager->CreateRewriter(
          mock_create_rewriter_client.BindNewPipeAndPassRemote(),
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

    rewriter_remote->Rewrite(kInputString, kContextString,
                             mock_responder.BindNewPipeAndPassRemote());
    run_loop.Run();
  }
};

TEST_F(AIRewriterTest, CreateRewriterNoService) {
  SetupNullOptimizationGuideKeyedService();

  MockCreateRewriterClient mock_create_rewriter_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_rewriter_client, OnError(_, _))
      .WillOnce(testing::Invoke([&](blink::mojom::AIManagerCreateClientError
                                        error,
                                    blink::mojom::QuotaErrorInfoPtr
                                        quota_error_info) {
        ASSERT_EQ(
            error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
        run_loop.Quit();
      }));

  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateRewriter(
      mock_create_rewriter_client.BindNewPipeAndPassRemote(),
      GetDefaultOptions());
  run_loop.Run();
}

TEST_F(AIRewriterTest, CreateRewriterModelNotEligible) {
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

  MockCreateRewriterClient mock_create_rewriter_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_rewriter_client, OnError(_, _))
      .WillOnce(testing::Invoke([&](blink::mojom::AIManagerCreateClientError
                                        error,
                                    blink::mojom::QuotaErrorInfoPtr
                                        quota_error_info) {
        ASSERT_EQ(
            error,
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
        run_loop.Quit();
      }));

  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateRewriter(
      mock_create_rewriter_client.BindNewPipeAndPassRemote(),
      GetDefaultOptions());
  run_loop.Run();
}

TEST_F(AIRewriterTest, CreateRewriterRetryAfterConfigNotAvailableForFeature) {
  SetupMockOptimizationGuideKeyedService();
  // StartSession must be called twice.
  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) {
            // Returns a nullptr for the first call.
            return nullptr;
          }))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) {
            // Returns a MockSession for the second call.
            return std::make_unique<
                testing::NiceMock<optimization_guide::MockSession>>(&session_);
          }));

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
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  EXPECT_CALL(session_, GetExecutionInputSizeInTokens(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::MultimodalMessageReadView request_metadata,
              optimization_guide::OptimizationGuideModelSizeInTokenCallback
                  callback) {
            std::move(callback).Run(
                blink::mojom::kWritingAssistanceMaxInputTokenSize);
          }));

  mojo::Remote<blink::mojom::AIRewriter> rewriter_remote;
  MockCreateRewriterClient mock_create_rewriter_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_rewriter_client, OnResult(_))
      .WillOnce(testing::Invoke(
          [&](mojo::PendingRemote<::blink::mojom::AIRewriter> rewriter) {
            // Create rewriter should succeed.
            EXPECT_TRUE(rewriter);
            run_loop.Quit();
          }));

  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateRewriter(
      mock_create_rewriter_client.BindNewPipeAndPassRemote(),
      GetDefaultOptions());

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);
  // Send `kConfigNotAvailableForFeature` first to the observer.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kWritingAssistanceApi,
      optimization_guide::OnDeviceModelEligibilityReason::
          kConfigNotAvailableForFeature);

  // And then send `kConfigNotAvailableForFeature` to the observer.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kWritingAssistanceApi,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  // OnResult() should be called.
  run_loop.Run();
}

TEST_F(AIRewriterTest, CreateRewriterContextLimitExceededError) {
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();

  EXPECT_CALL(session_, GetExecutionInputSizeInTokens(_, _))
      .WillOnce(testing::Invoke(
          [](optimization_guide::MultimodalMessageReadView request_metadata,
             optimization_guide::OptimizationGuideModelSizeInTokenCallback
                 callback) {
            std::move(callback).Run(
                blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);
          }));

  MockCreateRewriterClient mock_create_rewriter_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_rewriter_client, OnError(_, _))
      .WillOnce(testing::Invoke([&](blink::mojom::AIManagerCreateClientError
                                        error,
                                    blink::mojom::QuotaErrorInfoPtr
                                        quota_error_info) {
        ASSERT_EQ(
            error,
            blink::mojom::AIManagerCreateClientError::kInitialInputTooLarge);
        ASSERT_TRUE(quota_error_info);
        ASSERT_EQ(quota_error_info->requested,
                  blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);
        ASSERT_EQ(quota_error_info->quota,
                  blink::mojom::kWritingAssistanceMaxInputTokenSize);
        run_loop.Quit();
      }));

  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateRewriter(
      mock_create_rewriter_client.BindNewPipeAndPassRemote(),
      GetDefaultOptions());
  run_loop.Run();
}

TEST_F(AIRewriterTest, CreateRewriterAbortAfterConfigNotAvailableForFeature) {
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

  auto mock_create_rewriter_client =
      std::make_unique<MockCreateRewriterClient>();
  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateRewriter(
      mock_create_rewriter_client->BindNewPipeAndPassRemote(),
      GetDefaultOptions());

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  // Reset `mock_create_rewriter_client` to abort the task of CreateRewriter().
  mock_create_rewriter_client.reset();

  // RemoveOnDeviceModelAvailabilityChangeObserver should be called.
  run_loop_for_remove_observer.Run();
}

TEST_F(AIRewriterTest, CanCreateDefaultOptions) {
  SetupMockOptimizationGuideKeyedService();
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
  SetupMockOptimizationGuideKeyedService();
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
  SetupMockOptimizationGuideKeyedService();
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

TEST_F(AIRewriterTest, RewriteDefault) {
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
  RunSimpleRewriteTest(blink::mojom::AIRewriterTone::kAsIs,
                       blink::mojom::AIRewriterFormat::kAsIs,
                       blink::mojom::AIRewriterLength::kAsIs);
}

TEST_F(AIRewriterTest, RewriteWithOptions) {
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
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
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
  auto rewriter_remote = GetAIRewriterRemote();

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
      .WillOnce(testing::Invoke([&](blink::mojom::ModelStreamingResponseStatus
                                        status,
                                    blink::mojom::QuotaErrorInfoPtr
                                        quota_error_info) {
        EXPECT_EQ(
            status,
            blink::mojom::ModelStreamingResponseStatus::kErrorInputTooLarge);
        ASSERT_TRUE(quota_error_info);
        ASSERT_EQ(quota_error_info->requested,
                  blink::mojom::kWritingAssistanceMaxInputTokenSize + 1);
        ASSERT_EQ(quota_error_info->quota,
                  blink::mojom::kWritingAssistanceMaxInputTokenSize);
        run_loop.Quit();
      }));

  rewriter_remote->Rewrite(kInputString, kContextString,
                           mock_responder.BindNewPipeAndPassRemote());
  run_loop.Run();
}

TEST_F(AIRewriterTest, ModelExecutionError) {
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

  auto rewriter_remote = GetAIRewriterRemote();
  AITestUtils::MockModelStreamingResponder mock_responder;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_responder, OnError(_, _))
      .WillOnce(testing::Invoke([&](blink::mojom::ModelStreamingResponseStatus
                                        status,
                                    blink::mojom::QuotaErrorInfoPtr
                                        quota_error_info) {
        EXPECT_EQ(
            status,
            blink::mojom::ModelStreamingResponseStatus::kErrorPermissionDenied);
        run_loop.Quit();
      }));

  rewriter_remote->Rewrite(kInputString, kContextString,
                           mock_responder.BindNewPipeAndPassRemote());
  run_loop.Run();
}

TEST_F(AIRewriterTest, RewriteMultipleResponse) {
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

  auto rewriter_remote = GetAIRewriterRemote();
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

  rewriter_remote->Rewrite(kInputString, kContextString,
                           mock_responder.BindNewPipeAndPassRemote());
  run_loop.Run();
}

TEST_F(AIRewriterTest, MultipleRewrite) {
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
            auto expect = GetExecuteRequest("test context 2", "input string 2");
            EXPECT_THAT(request, EqualsProto(expect));
            callback.Run(CreateExecutionResult("Result text 2",
                                               /*is_complete=*/true));
          }));

  auto rewriter_remote = GetAIRewriterRemote();
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

    rewriter_remote->Rewrite(kInputString, kContextString,
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

    rewriter_remote->Rewrite("input string 2", "test context 2",
                             mock_responder.BindNewPipeAndPassRemote());
    run_loop.Run();
  }
}

TEST_F(AIRewriterTest, ResponderDisconnected) {
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

  auto rewriter_remote = GetAIRewriterRemote();
  std::unique_ptr<AITestUtils::MockModelStreamingResponder> mock_responder =
      std::make_unique<AITestUtils::MockModelStreamingResponder>();
  rewriter_remote->Rewrite(kInputString, kContextString,
                           mock_responder->BindNewPipeAndPassRemote());
  mock_responder.reset();
  // Call RunUntilIdle() to disconnect the ModelStreamingResponder mojo remote
  // interface in AIRewriter.
  task_environment()->RunUntilIdle();

  run_loop_for_callback.Run();
  ASSERT_TRUE(streaming_callback);
  streaming_callback.Run(CreateExecutionResult("Result text",
                                               /*is_complete=*/true));
  task_environment()->RunUntilIdle();
}

TEST_F(AIRewriterTest, RewriterDisconnected) {
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

  auto rewriter_remote = GetAIRewriterRemote();
  AITestUtils::MockModelStreamingResponder mock_responder;
  base::RunLoop run_loop_for_response;
  EXPECT_CALL(mock_responder, OnError(_, _))
      .WillOnce(testing::Invoke([&](blink::mojom::ModelStreamingResponseStatus
                                        status,
                                    blink::mojom::QuotaErrorInfoPtr
                                        quota_error_info) {
        EXPECT_EQ(
            status,
            blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
        run_loop_for_response.Quit();
      }));

  rewriter_remote->Rewrite(kInputString, kContextString,
                           mock_responder.BindNewPipeAndPassRemote());

  run_loop_for_callback.Run();

  // Disconnect the rewriter handle.
  rewriter_remote.reset();

  // Call RunUntilIdle() to destroy AIRewriter.
  task_environment()->RunUntilIdle();

  ASSERT_TRUE(streaming_callback);
  streaming_callback.Run(CreateExecutionResult("Result text",
                                               /*is_complete=*/true));
  run_loop_for_response.Run();
}

TEST_F(AIRewriterTest, MeasureUsage) {
  uint64_t expected_usage = 100;
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
  auto rewriter_remote = GetAIRewriterRemote();

  EXPECT_CALL(session_, GetExecutionInputSizeInTokens(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::MultimodalMessageReadView request_metadata,
              optimization_guide::OptimizationGuideModelSizeInTokenCallback
                  callback) { std::move(callback).Run(expected_usage); }));
  base::test::TestFuture<std::optional<uint32_t>> future;
  rewriter_remote->MeasureUsage(kInputString, kContextString,
                                future.GetCallback());
  ASSERT_EQ(future.Get<0>(), expected_usage);
}

TEST_F(AIRewriterTest, MeasureUsageFails) {
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
  auto rewriter_remote = GetAIRewriterRemote();

  EXPECT_CALL(session_, GetExecutionInputSizeInTokens(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::MultimodalMessageReadView request_metadata,
              optimization_guide::OptimizationGuideModelSizeInTokenCallback
                  callback) { std::move(callback).Run(std::nullopt); }));
  base::test::TestFuture<std::optional<uint32_t>> future;
  rewriter_remote->MeasureUsage(kInputString, kContextString,
                                future.GetCallback());
  ASSERT_EQ(future.Get<0>(), std::nullopt);
}

TEST_F(AIRewriterTest, Priority) {
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();

  EXPECT_CALL(session_,
              SetPriority(on_device_model::mojom::Priority::kForeground));
  auto remote = GetAIRewriterRemote();

  EXPECT_CALL(session_,
              SetPriority(on_device_model::mojom::Priority::kBackground));
  main_rfh()->GetRenderWidgetHost()->GetView()->Hide();

  EXPECT_CALL(session_,
              SetPriority(on_device_model::mojom::Priority::kForeground));
  main_rfh()->GetRenderWidgetHost()->GetView()->Show();
}

}  // namespace

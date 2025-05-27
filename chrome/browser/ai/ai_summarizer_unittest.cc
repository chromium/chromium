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
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/summarize.pb.h"
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

optimization_guide::OptimizationGuideModelStreamingExecutionResult
CreateExecutionResult(std::string_view output, bool is_complete) {
  optimization_guide::proto::StringValue response;
  *response.mutable_value() = output;
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

blink::mojom::AISummarizerCreateOptionsPtr GetDefaultOptions() {
  return blink::mojom::AISummarizerCreateOptions::New(
      kSharedContextString, blink::mojom::AISummarizerType::kTLDR,
      blink::mojom::AISummarizerFormat::kPlainText,
      blink::mojom::AISummarizerLength::kMedium,
      /*expected_input_languages=*/std::vector<AILanguageCodePtr>(),
      /*expected_context_languages=*/std::vector<AILanguageCodePtr>(),
      /*output_language=*/AILanguageCode::New(""));
}

// Get a request proto matching that expected for ExecuteModel() calls.
optimization_guide::proto::SummarizeRequest GetExecuteRequest(
    std::string_view context_string = kContextString,
    std::string_view article_string = kInputString) {
  optimization_guide::proto::SummarizeRequest request;
  request.set_context(
      AISummarizer::CombineContexts(kSharedContextString, context_string));
  request.set_allocated_options(
      AISummarizer::ToProtoOptions(GetDefaultOptions()).release());
  request.set_article(article_string);
  return request;
}

class AISummarizerTest : public AITestUtils::AITestBase {
 protected:
  mojo::Remote<blink::mojom::AISummarizer> GetAISummarizerRemote() {
    mojo::Remote<blink::mojom::AISummarizer> summarizer_remote;

    MockCreateSummarizerClient mock_create_summarizer_client;
    base::RunLoop run_loop;
    EXPECT_CALL(mock_create_summarizer_client, OnResult(_))
        .WillOnce(testing::Invoke(
            [&](mojo::PendingRemote<::blink::mojom::AISummarizer> summarizer) {
              EXPECT_TRUE(summarizer);
              summarizer_remote = mojo::Remote<blink::mojom::AISummarizer>(
                  std::move(summarizer));
              run_loop.Quit();
            }));

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
    auto expected = GetExecuteRequest();
    const auto options = blink::mojom::AISummarizerCreateOptions::New(
        kSharedContextString, type, format, length,
        /*expected_input_languages=*/std::vector<AILanguageCodePtr>(),
        /*expected_context_languages=*/std::vector<AILanguageCodePtr>(),
        /*output_language=*/AILanguageCode::New(""));
    expected.set_allocated_options(
        AISummarizer::ToProtoOptions(options).release());
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

    mojo::Remote<blink::mojom::AISummarizer> summarizer_remote;
    {
      MockCreateSummarizerClient mock_create_summarizer_client;
      base::RunLoop run_loop;
      EXPECT_CALL(mock_create_summarizer_client, OnResult(_))
          .WillOnce(testing::Invoke(
              [&](mojo::PendingRemote<::blink::mojom::AISummarizer>
                      Summarizer) {
                EXPECT_TRUE(Summarizer);
                summarizer_remote = mojo::Remote<blink::mojom::AISummarizer>(
                    std::move(Summarizer));
                run_loop.Quit();
              }));

      mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
      ai_manager->CreateSummarizer(
          mock_create_summarizer_client.BindNewPipeAndPassRemote(),
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

    summarizer_remote->Summarize(kInputString, kContextString,
                                 mock_responder.BindNewPipeAndPassRemote());
    run_loop.Run();
  }
};

TEST(AISummarizerStandaloneTest, CombineContexts) {
  EXPECT_EQ("", AISummarizer::CombineContexts("", ""));
  EXPECT_EQ("a\n", AISummarizer::CombineContexts("a", ""));
  EXPECT_EQ("b\n", AISummarizer::CombineContexts("", "b"));
  EXPECT_EQ("a b\n", AISummarizer::CombineContexts("a", "b"));
}

TEST_F(AISummarizerTest, CanCreateDefaultOptions) {
  SetupMockOptimizationGuideKeyedService();
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
  base::MockCallback<AIManager::CanCreateSummarizerCallback> callback;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::kAvailable));
  GetAIManagerInterface()->CanCreateSummarizer(std::move(options),
                                               callback.Get());
}

TEST_F(AISummarizerTest, CanCreateUnIsLanguagesSupported) {
  SetupMockOptimizationGuideKeyedService();
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

TEST_F(AISummarizerTest, CreateSummarizerNoService) {
  SetupNullOptimizationGuideKeyedService();
  MockCreateSummarizerClient mock_create_summarizer_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_summarizer_client, OnError(_, _))
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
  ai_manager->CreateSummarizer(
      mock_create_summarizer_client.BindNewPipeAndPassRemote(),
      GetDefaultOptions());
  run_loop.Run();
}

TEST_F(AISummarizerTest, CreateSummarizerModelNotEligible) {
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

  MockCreateSummarizerClient mock_create_summarizer_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_summarizer_client, OnError(_, _))
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
  ai_manager->CreateSummarizer(
      mock_create_summarizer_client.BindNewPipeAndPassRemote(),
      GetDefaultOptions());
  run_loop.Run();
}

TEST_F(AISummarizerTest,
       CreateSummarizerRetryAfterConfigNotAvailableForFeature) {
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

  mojo::Remote<blink::mojom::AISummarizer> summarizer_remote;
  MockCreateSummarizerClient mock_create_summarizer_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_summarizer_client, OnResult(_))
      .WillOnce(testing::Invoke(
          [&](mojo::PendingRemote<::blink::mojom::AISummarizer> summarizer) {
            // Create Summarizer should succeed.
            EXPECT_TRUE(summarizer);
            run_loop.Quit();
          }));

  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateSummarizer(
      mock_create_summarizer_client.BindNewPipeAndPassRemote(),
      GetDefaultOptions());

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);
  // Send `kConfigNotAvailableForFeature` first to the observer.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kSummarize,
      optimization_guide::OnDeviceModelEligibilityReason::
          kConfigNotAvailableForFeature);

  // And then send `kConfigNotAvailableForFeature` to the observer.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kSummarize,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  // OnResult() should be called.
  run_loop.Run();
}

TEST_F(AISummarizerTest, CreateSummarizerContextLimitExceededError) {
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

  MockCreateSummarizerClient mock_create_summarizer_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_summarizer_client, OnError(_, _))
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
  ai_manager->CreateSummarizer(
      mock_create_summarizer_client.BindNewPipeAndPassRemote(),
      GetDefaultOptions());
  run_loop.Run();
}

TEST_F(AISummarizerTest,
       CreateSummarizerAbortAfterConfigNotAvailableForFeature) {
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

  auto mock_create_summarizer_client =
      std::make_unique<MockCreateSummarizerClient>();
  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateSummarizer(
      mock_create_summarizer_client->BindNewPipeAndPassRemote(),
      GetDefaultOptions());

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  // Reset `mock_create_summarizer_client` to abort the task of
  // CreateSummarizer().
  mock_create_summarizer_client.reset();

  // RemoveOnDeviceModelAvailabilityChangeObserver should be called.
  run_loop_for_remove_observer.Run();
}

TEST_F(AISummarizerTest, SummarizeDefault) {
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
  RunSimpleSummarizeTest(blink::mojom::AISummarizerType::kTLDR,
                         blink::mojom::AISummarizerFormat::kPlainText,
                         blink::mojom::AISummarizerLength::kMedium);
}

TEST_F(AISummarizerTest, SummarizeWithOptions) {
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
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
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
  auto summarizer_remote = GetAISummarizerRemote();

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

  summarizer_remote->Summarize(kInputString, kContextString,
                               mock_responder.BindNewPipeAndPassRemote());
  run_loop.Run();
}

TEST_F(AISummarizerTest, ModelExecutionError) {
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

  auto summarizer_remote = GetAISummarizerRemote();
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

  summarizer_remote->Summarize(kInputString, kContextString,
                               mock_responder.BindNewPipeAndPassRemote());
  run_loop.Run();
}

TEST_F(AISummarizerTest, SummarizeMultipleResponse) {
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

  auto summarizer_remote = GetAISummarizerRemote();
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

  summarizer_remote->Summarize(kInputString, kContextString,
                               mock_responder.BindNewPipeAndPassRemote());
  run_loop.Run();
}

TEST_F(AISummarizerTest, MultipleSummarize) {
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
            EXPECT_THAT(request, EqualsProto(GetExecuteRequest(
                                     "test context 2", "input string 2")));
            callback.Run(CreateExecutionResult("Result text 2",
                                               /*is_complete=*/true));
          }));

  auto summarizer_remote = GetAISummarizerRemote();
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

    summarizer_remote->Summarize(kInputString, kContextString,
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

    summarizer_remote->Summarize("input string 2", "test context 2",
                                 mock_responder.BindNewPipeAndPassRemote());
    run_loop.Run();
  }
}

TEST_F(AISummarizerTest, ResponderDisconnected) {
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

  auto summarizer_remote = GetAISummarizerRemote();
  std::unique_ptr<AITestUtils::MockModelStreamingResponder> mock_responder =
      std::make_unique<AITestUtils::MockModelStreamingResponder>();
  summarizer_remote->Summarize(kInputString, kContextString,
                               mock_responder->BindNewPipeAndPassRemote());
  mock_responder.reset();
  // Call RunUntilIdle() to disconnect the ModelStreamingResponder mojo remote
  // interface in AISummarizer.
  task_environment()->RunUntilIdle();

  run_loop_for_callback.Run();
  ASSERT_TRUE(streaming_callback);
  streaming_callback.Run(CreateExecutionResult("Result text",
                                               /*is_complete=*/true));
  task_environment()->RunUntilIdle();
}

TEST_F(AISummarizerTest, SummarizerDisconnected) {
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

  auto summarizer_remote = GetAISummarizerRemote();
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

  summarizer_remote->Summarize(kInputString, kContextString,
                               mock_responder.BindNewPipeAndPassRemote());

  run_loop_for_callback.Run();

  // Disconnect the Summarizer handle.
  summarizer_remote.reset();

  // Call RunUntilIdle() to destroy AISummarizer.
  task_environment()->RunUntilIdle();

  ASSERT_TRUE(streaming_callback);
  streaming_callback.Run(CreateExecutionResult("Result text",
                                               /*is_complete=*/true));
  run_loop_for_response.Run();
}

TEST_F(AISummarizerTest, MeasureUsage) {
  uint64_t expected_usage = 100;
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
  auto summarizer_remote = GetAISummarizerRemote();

  EXPECT_CALL(session_, GetExecutionInputSizeInTokens(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::MultimodalMessageReadView request_metadata,
              optimization_guide::OptimizationGuideModelSizeInTokenCallback
                  callback) { std::move(callback).Run(expected_usage); }));
  base::test::TestFuture<std::optional<uint32_t>> future;
  summarizer_remote->MeasureUsage(kInputString, kContextString,
                                  future.GetCallback());
  ASSERT_EQ(future.Get<0>(), expected_usage);
}

TEST_F(AISummarizerTest, MeasureUsageFails) {
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
  auto summarizer_remote = GetAISummarizerRemote();

  EXPECT_CALL(session_, GetExecutionInputSizeInTokens(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::MultimodalMessageReadView request_metadata,
              optimization_guide::OptimizationGuideModelSizeInTokenCallback
                  callback) { std::move(callback).Run(std::nullopt); }));
  base::test::TestFuture<std::optional<uint32_t>> future;
  summarizer_remote->MeasureUsage(kInputString, kContextString,
                                  future.GetCallback());
  ASSERT_EQ(future.Get<0>(), std::nullopt);
}

TEST_F(AISummarizerTest, Priority) {
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();

  EXPECT_CALL(session_,
              SetPriority(on_device_model::mojom::Priority::kForeground));
  auto remote = GetAISummarizerRemote();

  EXPECT_CALL(session_,
              SetPriority(on_device_model::mojom::Priority::kBackground));
  main_rfh()->GetRenderWidgetHost()->GetView()->Hide();

  EXPECT_CALL(session_,
              SetPriority(on_device_model::mojom::Priority::kForeground));
  main_rfh()->GetRenderWidgetHost()->GetView()->Show();
}

}  // namespace

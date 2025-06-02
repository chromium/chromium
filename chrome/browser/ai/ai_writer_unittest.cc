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

blink::mojom::AIWriterCreateOptionsPtr GetDefaultOptions() {
  return blink::mojom::AIWriterCreateOptions::New(
      kSharedContextString, blink::mojom::AIWriterTone::kNeutral,
      blink::mojom::AIWriterFormat::kPlainText,
      blink::mojom::AIWriterLength::kMedium,
      /*expected_input_languages=*/std::vector<AILanguageCodePtr>(),
      /*expected_context_languages=*/std::vector<AILanguageCodePtr>(),
      /*output_language=*/AILanguageCode::New(""));
}

// Get a request proto matching that expected for ExecuteModel() calls.
optimization_guide::proto::WritingAssistanceApiRequest GetExecuteRequest(
    std::string_view context_string = kContextString,
    std::string_view instructions_string = kInputString) {
  optimization_guide::proto::WritingAssistanceApiRequest request;
  request.set_context(context_string);
  request.set_allocated_options(
      AIWriter::ToProtoOptions(GetDefaultOptions()).release());
  request.set_instructions(instructions_string);
  request.set_shared_context(kSharedContextString);
  return request;
}

class AIWriterTest : public AITestUtils::AITestBase {
 protected:
  mojo::Remote<blink::mojom::AIWriter> GetAIWriterRemote() {
    mojo::Remote<blink::mojom::AIWriter> writer_remote;

    MockCreateWriterClient mock_create_writer_client;
    base::RunLoop run_loop;
    EXPECT_CALL(mock_create_writer_client, OnResult(_))
        .WillOnce(testing::Invoke(
            [&](mojo::PendingRemote<::blink::mojom::AIWriter> writer) {
              EXPECT_TRUE(writer);
              writer_remote =
                  mojo::Remote<blink::mojom::AIWriter>(std::move(writer));
              run_loop.Quit();
            }));

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
    auto expected = GetExecuteRequest();
    const auto options = blink::mojom::AIWriterCreateOptions::New(
        kSharedContextString, tone, format, length,
        /*expected_input_languages=*/std::vector<AILanguageCodePtr>(),
        /*expected_context_languages=*/std::vector<AILanguageCodePtr>(),
        /*output_language=*/AILanguageCode::New(""));
    expected.set_allocated_options(AIWriter::ToProtoOptions(options).release());
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

    mojo::Remote<blink::mojom::AIWriter> writer_remote;
    {
      MockCreateWriterClient mock_create_writer_client;
      base::RunLoop run_loop;
      EXPECT_CALL(mock_create_writer_client, OnResult(_))
          .WillOnce(testing::Invoke(
              [&](mojo::PendingRemote<::blink::mojom::AIWriter> writer) {
                EXPECT_TRUE(writer);
                writer_remote =
                    mojo::Remote<blink::mojom::AIWriter>(std::move(writer));
                run_loop.Quit();
              }));

      mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
      ai_manager->CreateWriter(
          mock_create_writer_client.BindNewPipeAndPassRemote(),
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

    writer_remote->Write(kInputString, kContextString,
                         mock_responder.BindNewPipeAndPassRemote());
    run_loop.Run();
  }
};

TEST_F(AIWriterTest, CanCreateDefaultOptions) {
  SetupMockOptimizationGuideKeyedService();
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
  base::MockCallback<AIManager::CanCreateWriterCallback> callback;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::kAvailable));
  GetAIManagerInterface()->CanCreateWriter(std::move(options), callback.Get());
}

TEST_F(AIWriterTest, CanCreateUnIsLanguagesSupported) {
  SetupMockOptimizationGuideKeyedService();
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

TEST_F(AIWriterTest, CreateWriterNoService) {
  SetupNullOptimizationGuideKeyedService();
  MockCreateWriterClient mock_create_writer_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_writer_client, OnError(_, _))
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
  ai_manager->CreateWriter(mock_create_writer_client.BindNewPipeAndPassRemote(),
                           GetDefaultOptions());
  run_loop.Run();
}

TEST_F(AIWriterTest, CreateWriterModelNotEligible) {
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

  MockCreateWriterClient mock_create_writer_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_writer_client, OnError(_, _))
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
  ai_manager->CreateWriter(mock_create_writer_client.BindNewPipeAndPassRemote(),
                           GetDefaultOptions());
  run_loop.Run();
}

TEST_F(AIWriterTest, CreateWriterRetryAfterConfigNotAvailableForFeature) {
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

  mojo::Remote<blink::mojom::AIWriter> writer_remote;
  MockCreateWriterClient mock_create_writer_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_writer_client, OnResult(_))
      .WillOnce(testing::Invoke(
          [&](mojo::PendingRemote<::blink::mojom::AIWriter> writer) {
            // Create writer should succeed.
            EXPECT_TRUE(writer);
            run_loop.Quit();
          }));

  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateWriter(mock_create_writer_client.BindNewPipeAndPassRemote(),
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

TEST_F(AIWriterTest, CreateWriterAbortAfterConfigNotAvailableForFeature) {
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

  auto mock_create_writer_client = std::make_unique<MockCreateWriterClient>();
  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateWriter(
      mock_create_writer_client->BindNewPipeAndPassRemote(),
      GetDefaultOptions());

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  // Reset `mock_create_writer_client` to abort the task of CreateWriter().
  mock_create_writer_client.reset();

  // RemoveOnDeviceModelAvailabilityChangeObserver should be called.
  run_loop_for_remove_observer.Run();
}

TEST_F(AIWriterTest, CreateWriterContextLimitExceededError) {
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

  MockCreateWriterClient mock_create_writer_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_writer_client, OnError(_, _))
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
  ai_manager->CreateWriter(mock_create_writer_client.BindNewPipeAndPassRemote(),
                           GetDefaultOptions());
  run_loop.Run();
}

TEST_F(AIWriterTest, WriteDefault) {
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
  RunSimpleWriteTest(blink::mojom::AIWriterTone::kNeutral,
                     blink::mojom::AIWriterFormat::kPlainText,
                     blink::mojom::AIWriterLength::kMedium);
}

TEST_F(AIWriterTest, WriteWithOptions) {
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
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
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
  auto writer_remote = GetAIWriterRemote();

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

  writer_remote->Write(kInputString, kContextString,
                       mock_responder.BindNewPipeAndPassRemote());
  run_loop.Run();
}

TEST_F(AIWriterTest, ModelExecutionError) {
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

  auto writer_remote = GetAIWriterRemote();
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

  writer_remote->Write(kInputString, kContextString,
                       mock_responder.BindNewPipeAndPassRemote());
  run_loop.Run();
}

TEST_F(AIWriterTest, WriteMultipleResponse) {
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

  auto writer_remote = GetAIWriterRemote();
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

  writer_remote->Write(kInputString, kContextString,
                       mock_responder.BindNewPipeAndPassRemote());
  run_loop.Run();
}

TEST_F(AIWriterTest, MultipleWrite) {
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

  auto writer_remote = GetAIWriterRemote();
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

    writer_remote->Write(kInputString, kContextString,
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

    writer_remote->Write("input string 2", "test context 2",
                         mock_responder.BindNewPipeAndPassRemote());
    run_loop.Run();
  }
}

TEST_F(AIWriterTest, ResponderDisconnected) {
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

  auto writer_remote = GetAIWriterRemote();
  std::unique_ptr<AITestUtils::MockModelStreamingResponder> mock_responder =
      std::make_unique<AITestUtils::MockModelStreamingResponder>();
  writer_remote->Write(kInputString, kContextString,
                       mock_responder->BindNewPipeAndPassRemote());
  mock_responder.reset();
  // Call RunUntilIdle() to disconnect the ModelStreamingResponder mojo remote
  // interface in AIWriter.
  task_environment()->RunUntilIdle();

  run_loop_for_callback.Run();
  ASSERT_TRUE(streaming_callback);
  streaming_callback.Run(CreateExecutionResult("Result text",
                                               /*is_complete=*/true));
  task_environment()->RunUntilIdle();
}

TEST_F(AIWriterTest, WriterDisconnected) {
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

  auto writer_remote = GetAIWriterRemote();
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

  writer_remote->Write(kInputString, kContextString,
                       mock_responder.BindNewPipeAndPassRemote());

  run_loop_for_callback.Run();

  // Disconnect the writer handle.
  writer_remote.reset();

  // Call RunUntilIdle() to destroy AIWriter.
  task_environment()->RunUntilIdle();

  ASSERT_TRUE(streaming_callback);
  streaming_callback.Run(CreateExecutionResult("Result text",
                                               /*is_complete=*/true));
  run_loop_for_response.Run();
}

TEST_F(AIWriterTest, MeasureUsage) {
  uint64_t expected_usage = 100;
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
  auto writer_remote = GetAIWriterRemote();

  EXPECT_CALL(session_, GetExecutionInputSizeInTokens(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::MultimodalMessageReadView request_metadata,
              optimization_guide::OptimizationGuideModelSizeInTokenCallback
                  callback) { std::move(callback).Run(expected_usage); }));
  base::test::TestFuture<std::optional<uint32_t>> future;
  writer_remote->MeasureUsage(kInputString, kContextString,
                              future.GetCallback());
  ASSERT_EQ(future.Get<0>(), expected_usage);
}

TEST_F(AIWriterTest, MeasureUsageFails) {
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();
  auto writer_remote = GetAIWriterRemote();

  EXPECT_CALL(session_, GetExecutionInputSizeInTokens(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::MultimodalMessageReadView request_metadata,
              optimization_guide::OptimizationGuideModelSizeInTokenCallback
                  callback) { std::move(callback).Run(std::nullopt); }));
  base::test::TestFuture<std::optional<uint32_t>> future;
  writer_remote->MeasureUsage(kInputString, kContextString,
                              future.GetCallback());
  ASSERT_EQ(future.Get<0>(), std::nullopt);
}

TEST_F(AIWriterTest, Priority) {
  SetupMockOptimizationGuideKeyedService();
  SetupMockSession();

  EXPECT_CALL(session_,
              SetPriority(on_device_model::mojom::Priority::kForeground));
  auto remote = GetAIWriterRemote();

  EXPECT_CALL(session_,
              SetPriority(on_device_model::mojom::Priority::kBackground));
  main_rfh()->GetRenderWidgetHost()->GetView()->Hide();

  EXPECT_CALL(session_,
              SetPriority(on_device_model::mojom::Priority::kForeground));
  main_rfh()->GetRenderWidgetHost()->GetView()->Show();
}

}  // namespace

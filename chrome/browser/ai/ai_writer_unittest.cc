// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_writer.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ai/ai_manager_keyed_service_factory.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

using ::testing::_;

namespace {

constexpr char kSharedContextString[] = "test shared context";
constexpr char kContextString[] = "test context";
constexpr char kConcatenatedContextString[] =
    "test shared context\ntest context";
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

 private:
  mojo::Receiver<blink::mojom::AIManagerCreateWriterClient> receiver_{this};
};

optimization_guide::OptimizationGuideModelStreamingExecutionResult
CreateExecutionResult(std::string_view output, bool is_complete) {
  optimization_guide::proto::ComposeResponse response;
  *response.mutable_output() = output;
  std::string serialized_metadata;
  response.SerializeToString(&serialized_metadata);
  optimization_guide::proto::Any any;
  any.set_value(serialized_metadata);
  any.set_type_url(AITestUtils::GetTypeURLForProto(response.GetTypeName()));
  return optimization_guide::OptimizationGuideModelStreamingExecutionResult(
      optimization_guide::StreamingResponse{
          .response = any,
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

void CheckComposeRequestContext(
    const google::protobuf::MessageLite& request_metadata,
    const std::string& expected_context_string) {
  const optimization_guide::proto::ComposeRequest* request =
      static_cast<const optimization_guide::proto::ComposeRequest*>(
          &request_metadata);
  EXPECT_THAT(request->page_metadata().page_inner_text(),
              expected_context_string);
  EXPECT_THAT(request->page_metadata().trimmed_page_inner_text(),
              expected_context_string);
}
void CheckComposeRequestUserInput(
    const google::protobuf::MessageLite& request_metadata,
    const std::string& expected_user_input) {
  const optimization_guide::proto::ComposeRequest* request =
      static_cast<const optimization_guide::proto::ComposeRequest*>(
          &request_metadata);
  EXPECT_THAT(request->generate_params().user_input(), expected_user_input);
}

}  // namespace

class AIWriterTest : public AITestUtils::AITestBase {};

TEST_F(AIWriterTest, CreateWriterNoService) {
  SetupNullOptimizationGuideKeyedService();

  MockCreateWriterClient mock_create_writer_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_writer_client, OnResult(_))
      .WillOnce(testing::Invoke(
          [&](mojo::PendingRemote<::blink::mojom::AIWriter> writer) {
            EXPECT_FALSE(writer);
            run_loop.Quit();
          }));

  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateWriter(
      mock_create_writer_client.BindNewPipeAndPassRemote(),
      blink::mojom::AIWriterCreateOptions::New(kSharedContextString));
  run_loop.Run();
}

TEST_F(AIWriterTest, CreateWriterModelNotAvailable) {
  SetupMockOptimizationGuideKeyedService();
  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) { return nullptr; }));
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanCreateOnDeviceSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              raw_ptr<optimization_guide::OnDeviceModelEligibilityReason>
                  debug_reason) {
            *debug_reason = optimization_guide::OnDeviceModelEligibilityReason::
                kModelNotAvailable;
            return false;
          }));

  MockCreateWriterClient mock_create_writer_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_writer_client, OnResult(_))
      .WillOnce(testing::Invoke(
          [&](mojo::PendingRemote<::blink::mojom::AIWriter> writer) {
            EXPECT_FALSE(writer);
            run_loop.Quit();
          }));

  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateWriter(
      mock_create_writer_client.BindNewPipeAndPassRemote(),
      blink::mojom::AIWriterCreateOptions::New(kSharedContextString));
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
            return std::make_unique<optimization_guide::MockSession>();
          }));

  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanCreateOnDeviceSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              raw_ptr<optimization_guide::OnDeviceModelEligibilityReason>
                  debug_reason) {
            // Setting kConfigNotAvailableForFeature should trigger retry.
            *debug_reason = optimization_guide::OnDeviceModelEligibilityReason::
                kConfigNotAvailableForFeature;
            return false;
          }));

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
  ai_manager->CreateWriter(
      mock_create_writer_client.BindNewPipeAndPassRemote(),
      blink::mojom::AIWriterCreateOptions::New(kSharedContextString));

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);
  // Send `kConfigNotAvailableForFeature` first to the observer.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kCompose,
      optimization_guide::OnDeviceModelEligibilityReason::
          kConfigNotAvailableForFeature);

  // And then send `kConfigNotAvailableForFeature` to the observer.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kCompose,
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
              CanCreateOnDeviceSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              raw_ptr<optimization_guide::OnDeviceModelEligibilityReason>
                  debug_reason) {
            // Setting kConfigNotAvailableForFeature should trigger retry.
            *debug_reason = optimization_guide::OnDeviceModelEligibilityReason::
                kConfigNotAvailableForFeature;
            return false;
          }));

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
      blink::mojom::AIWriterCreateOptions::New(kSharedContextString));

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  // Reset `mock_create_writer_client` to abort the task of CreateWriter().
  mock_create_writer_client.reset();

  // RemoveOnDeviceModelAvailabilityChangeObserver should be called.
  run_loop_for_remove_observer.Run();
}

TEST_F(AIWriterTest, ContextDestroyed) {
  SetupMockOptimizationGuideKeyedService();
  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) {
            return std::make_unique<optimization_guide::MockSession>();
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
        blink::mojom::AIWriterCreateOptions::New(kSharedContextString));
    run_loop.Run();
  }

  // Resetting mock host must delete the AIWriter.
  base::RunLoop run_loop;
  writer_remote.set_disconnect_handler(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
  ResetMockHost();
  run_loop.Run();
}

TEST_F(AIWriterTest, SimpleWrite) {
  SetupMockOptimizationGuideKeyedService();
  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke([&](optimization_guide::ModelBasedCapabilityKey
                                        feature,
                                    const std::optional<
                                        optimization_guide::
                                            SessionConfigParams>&
                                        config_params) {
        EXPECT_EQ(feature,
                  optimization_guide::ModelBasedCapabilityKey::kCompose);
        auto session = std::make_unique<optimization_guide::MockSession>();
        EXPECT_CALL(*session, AddContext(_))
            .WillOnce(testing::Invoke(
                [](const google::protobuf::MessageLite& request_metadata) {
                  CheckComposeRequestContext(request_metadata,
                                             kConcatenatedContextString);
                }));
        EXPECT_CALL(*session, ExecuteModel(_, _))
            .WillOnce(testing::Invoke(
                [](const google::protobuf::MessageLite& request_metadata,
                   optimization_guide::
                       OptimizationGuideModelExecutionResultStreamingCallback
                           callback) {
                  CheckComposeRequestUserInput(request_metadata, kInputString);
                  callback.Run(CreateExecutionResult("Result text",
                                                     /*is_complete=*/true));
                }));
        return session;
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
        blink::mojom::AIWriterCreateOptions::New(kSharedContextString));
    run_loop.Run();
  }
  AITestUtils::MockModelStreamingResponder mock_responder;

  base::RunLoop run_loop;
  EXPECT_CALL(mock_responder, OnResponse(_, _, _))
      .WillOnce(
          testing::Invoke([&](blink::mojom::ModelStreamingResponseStatus status,
                              const std::optional<std::string>& text,
                              std::optional<uint64_t> current_tokens) {
            EXPECT_THAT(text, "Result text");
            EXPECT_EQ(status,
                      blink::mojom::ModelStreamingResponseStatus::kOngoing);
          }))
      .WillOnce(
          testing::Invoke([&](blink::mojom::ModelStreamingResponseStatus status,
                              const std::optional<std::string>& text,
                              std::optional<uint64_t> current_tokens) {
            EXPECT_EQ(status,
                      blink::mojom::ModelStreamingResponseStatus::kComplete);
            run_loop.Quit();
          }));

  writer_remote->Write(kInputString, kContextString,
                       mock_responder.BindNewPipeAndPassRemote());
  run_loop.Run();
}

TEST_F(AIWriterTest, WriteError) {
  SetupMockOptimizationGuideKeyedService();
  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke([&](optimization_guide::ModelBasedCapabilityKey
                                        feature,
                                    const std::optional<
                                        optimization_guide::
                                            SessionConfigParams>&
                                        config_params) {
        auto session = std::make_unique<optimization_guide::MockSession>();

        EXPECT_CALL(*session, AddContext(_))
            .WillOnce(testing::Invoke(
                [](const google::protobuf::MessageLite& request_metadata) {
                  CheckComposeRequestContext(request_metadata,
                                             kConcatenatedContextString);
                }));
        EXPECT_CALL(*session, ExecuteModel(_, _))
            .WillOnce(testing::Invoke(
                [](const google::protobuf::MessageLite& request_metadata,
                   optimization_guide::
                       OptimizationGuideModelExecutionResultStreamingCallback
                           callback) {
                  CheckComposeRequestUserInput(request_metadata, kInputString);
                  callback.Run(CreateExecutionErrorResult(
                      optimization_guide::OptimizationGuideModelExecutionError::
                          FromModelExecutionError(
                              optimization_guide::
                                  OptimizationGuideModelExecutionError::
                                      ModelExecutionError::kPermissionDenied)));
                }));
        return session;
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
        blink::mojom::AIWriterCreateOptions::New(kSharedContextString));
    run_loop.Run();
  }
  AITestUtils::MockModelStreamingResponder mock_responder;

  base::RunLoop run_loop;
  EXPECT_CALL(mock_responder, OnResponse(_, _, _))
      .WillOnce(testing::Invoke([&](blink::mojom::ModelStreamingResponseStatus
                                        status,
                                    const std::optional<std::string>& text,
                                    std::optional<uint64_t> current_tokens) {
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
  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke([&](optimization_guide::ModelBasedCapabilityKey
                                        feature,
                                    const std::optional<
                                        optimization_guide::
                                            SessionConfigParams>&
                                        config_params) {
        auto session = std::make_unique<optimization_guide::MockSession>();

        EXPECT_CALL(*session, AddContext(_))
            .WillOnce(testing::Invoke(
                [](const google::protobuf::MessageLite& request_metadata) {
                  CheckComposeRequestContext(request_metadata,
                                             kConcatenatedContextString);
                }));
        EXPECT_CALL(*session, ExecuteModel(_, _))
            .WillOnce(testing::Invoke(
                [](const google::protobuf::MessageLite& request_metadata,
                   optimization_guide::
                       OptimizationGuideModelExecutionResultStreamingCallback
                           callback) {
                  CheckComposeRequestUserInput(request_metadata, kInputString);

                  callback.Run(
                      CreateExecutionResult("Result ", /*is_complete=*/false));
                  callback.Run(
                      CreateExecutionResult("text", /*is_complete=*/true));
                }));
        return session;
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
        blink::mojom::AIWriterCreateOptions::New(kSharedContextString));
    run_loop.Run();
  }
  AITestUtils::MockModelStreamingResponder mock_responder;

  base::RunLoop run_loop;
  EXPECT_CALL(mock_responder, OnResponse(_, _, _))
      .WillOnce(
          testing::Invoke([&](blink::mojom::ModelStreamingResponseStatus status,
                              const std::optional<std::string>& text,
                              std::optional<uint64_t> current_tokens) {
            EXPECT_THAT(text, "Result ");
            EXPECT_EQ(status,
                      blink::mojom::ModelStreamingResponseStatus::kOngoing);
          }))
      .WillOnce(
          testing::Invoke([&](blink::mojom::ModelStreamingResponseStatus status,
                              const std::optional<std::string>& text,
                              std::optional<uint64_t> current_tokens) {
            EXPECT_THAT(text, "text");
            EXPECT_EQ(status,
                      blink::mojom::ModelStreamingResponseStatus::kOngoing);
          }))
      .WillOnce(
          testing::Invoke([&](blink::mojom::ModelStreamingResponseStatus status,
                              const std::optional<std::string>& text,
                              std::optional<uint64_t> current_tokens) {
            EXPECT_EQ(status,
                      blink::mojom::ModelStreamingResponseStatus::kComplete);
            run_loop.Quit();
          }));

  writer_remote->Write(kInputString, kContextString,
                       mock_responder.BindNewPipeAndPassRemote());
  run_loop.Run();
}

TEST_F(AIWriterTest, MultipleWrite) {
  SetupMockOptimizationGuideKeyedService();
  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke([&](optimization_guide::ModelBasedCapabilityKey
                                        feature,
                                    const std::optional<
                                        optimization_guide::
                                            SessionConfigParams>&
                                        config_params) {
        auto session = std::make_unique<optimization_guide::MockSession>();

        EXPECT_CALL(*session, AddContext(_))
            .WillOnce(testing::Invoke(
                [](const google::protobuf::MessageLite& request_metadata) {
                  CheckComposeRequestContext(request_metadata,
                                             kConcatenatedContextString);
                }))
            .WillOnce(testing::Invoke(
                [](const google::protobuf::MessageLite& request_metadata) {
                  CheckComposeRequestContext(
                      request_metadata, "test shared context\ntest context 2");
                }));
        EXPECT_CALL(*session, ExecuteModel(_, _))
            .WillOnce(testing::Invoke(
                [](const google::protobuf::MessageLite& request_metadata,
                   optimization_guide::
                       OptimizationGuideModelExecutionResultStreamingCallback
                           callback) {
                  CheckComposeRequestUserInput(request_metadata, kInputString);
                  callback.Run(CreateExecutionResult("Result text",
                                                     /*is_complete=*/true));
                }))
            .WillOnce(testing::Invoke(
                [](const google::protobuf::MessageLite& request_metadata,
                   optimization_guide::
                       OptimizationGuideModelExecutionResultStreamingCallback
                           callback) {
                  CheckComposeRequestUserInput(request_metadata,
                                               "input string 2");
                  callback.Run(CreateExecutionResult("Result text 2",
                                                     /*is_complete=*/true));
                }));
        return session;
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
        blink::mojom::AIWriterCreateOptions::New(kSharedContextString));
    run_loop.Run();
  }
  {
    AITestUtils::MockModelStreamingResponder mock_responder;
    base::RunLoop run_loop;
    EXPECT_CALL(mock_responder, OnResponse(_, _, _))
        .WillOnce(testing::Invoke(
            [&](blink::mojom::ModelStreamingResponseStatus status,
                const std::optional<std::string>& text,
                std::optional<uint64_t> current_tokens) {
              EXPECT_THAT(text, "Result text");
              EXPECT_EQ(status,
                        blink::mojom::ModelStreamingResponseStatus::kOngoing);
            }))
        .WillOnce(testing::Invoke(
            [&](blink::mojom::ModelStreamingResponseStatus status,
                const std::optional<std::string>& text,
                std::optional<uint64_t> current_tokens) {
              EXPECT_EQ(status,
                        blink::mojom::ModelStreamingResponseStatus::kComplete);
              run_loop.Quit();
            }));

    writer_remote->Write(kInputString, kContextString,
                         mock_responder.BindNewPipeAndPassRemote());
    run_loop.Run();
  }
  {
    AITestUtils::MockModelStreamingResponder mock_responder;
    base::RunLoop run_loop;
    EXPECT_CALL(mock_responder, OnResponse(_, _, _))
        .WillOnce(testing::Invoke(
            [&](blink::mojom::ModelStreamingResponseStatus status,
                const std::optional<std::string>& text,
                std::optional<uint64_t> current_tokens) {
              EXPECT_THAT(text, "Result text 2");
              EXPECT_EQ(status,
                        blink::mojom::ModelStreamingResponseStatus::kOngoing);
            }))
        .WillOnce(testing::Invoke(
            [&](blink::mojom::ModelStreamingResponseStatus status,
                const std::optional<std::string>& text,
                std::optional<uint64_t> current_tokens) {
              EXPECT_EQ(status,
                        blink::mojom::ModelStreamingResponseStatus::kComplete);
              run_loop.Quit();
            }));

    writer_remote->Write("input string 2", "test context 2",
                         mock_responder.BindNewPipeAndPassRemote());
    run_loop.Run();
  }
}

TEST_F(AIWriterTest, ResponderDisconnected) {
  SetupMockOptimizationGuideKeyedService();

  base::RunLoop run_loop_for_callback;
  optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
      streaming_callback;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke([&](optimization_guide::ModelBasedCapabilityKey
                                        feature,
                                    const std::optional<
                                        optimization_guide::
                                            SessionConfigParams>&
                                        config_params) {
        auto session = std::make_unique<optimization_guide::MockSession>();

        EXPECT_CALL(*session, AddContext(_))
            .WillOnce(testing::Invoke(
                [](const google::protobuf::MessageLite& request_metadata) {
                  CheckComposeRequestContext(request_metadata,
                                             kConcatenatedContextString);
                }));
        EXPECT_CALL(*session, ExecuteModel(_, _))
            .WillOnce(testing::Invoke(
                [&](const google::protobuf::MessageLite& request_metadata,
                    optimization_guide::
                        OptimizationGuideModelExecutionResultStreamingCallback
                            callback) {
                  CheckComposeRequestUserInput(request_metadata, kInputString);
                  streaming_callback = std::move(callback);
                  run_loop_for_callback.Quit();
                }));
        return session;
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
        blink::mojom::AIWriterCreateOptions::New(kSharedContextString));
    run_loop.Run();
  }
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

  base::RunLoop run_loop_for_callback;
  optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
      streaming_callback;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke([&](optimization_guide::ModelBasedCapabilityKey
                                        feature,
                                    const std::optional<
                                        optimization_guide::
                                            SessionConfigParams>&
                                        config_params) {
        auto session = std::make_unique<optimization_guide::MockSession>();

        EXPECT_CALL(*session, AddContext(_))
            .WillOnce(testing::Invoke(
                [](const google::protobuf::MessageLite& request_metadata) {
                  CheckComposeRequestContext(request_metadata,
                                             kConcatenatedContextString);
                }));
        EXPECT_CALL(*session, ExecuteModel(_, _))
            .WillOnce(testing::Invoke(
                [&](const google::protobuf::MessageLite& request_metadata,
                    optimization_guide::
                        OptimizationGuideModelExecutionResultStreamingCallback
                            callback) {
                  CheckComposeRequestUserInput(request_metadata, kInputString);
                  streaming_callback = std::move(callback);
                  run_loop_for_callback.Quit();
                }));
        return session;
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
        blink::mojom::AIWriterCreateOptions::New(kSharedContextString));
    run_loop.Run();
  }

  AITestUtils::MockModelStreamingResponder mock_responder;
  base::RunLoop run_loop_for_response;
  EXPECT_CALL(mock_responder, OnResponse(_, _, _))
      .WillOnce(testing::Invoke([&](blink::mojom::ModelStreamingResponseStatus
                                        status,
                                    const std::optional<std::string>& text,
                                    std::optional<uint64_t> current_tokens) {
        // The OnResponse must be called with kErrorSessionDestroyed.
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

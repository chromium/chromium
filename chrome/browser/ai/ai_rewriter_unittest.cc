// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_rewriter.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ai/ai_manager_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

using ::testing::_;
using ::testing::AtMost;
using ::testing::NiceMock;

namespace {

constexpr char kSharedContextString[] = "test shared context";
constexpr char kContextString[] = "test context";
constexpr char kConcatenatedContextString[] =
    "test shared context\ntest context";
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
              (mojo::PendingRemote<::blink::mojom::AIRewriter> writer),
              (override));

 private:
  mojo::Receiver<blink::mojom::AIManagerCreateRewriterClient> receiver_{this};
};

// TODO(crbug.com/358214322): Move MockResponder to a common utils file.
class MockResponder : public blink::mojom::ModelStreamingResponder {
 public:
  MockResponder() = default;
  ~MockResponder() override = default;
  MockResponder(const MockResponder&) = delete;
  MockResponder& operator=(const MockResponder&) = delete;

  mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void,
              OnResponse,
              (blink::mojom::ModelStreamingResponseStatus status,
               const std::optional<std::string>& text,
               std::optional<uint64_t> current_tokens),
              (override));

 private:
  mojo::Receiver<blink::mojom::ModelStreamingResponder> receiver_{this};
};

class MockSupportsUserData : public base::SupportsUserData {};

optimization_guide::OptimizationGuideModelStreamingExecutionResult
CreateExecutionResult(const std::string_view outpt, bool is_complete) {
  optimization_guide::proto::ComposeResponse response;
  *response.mutable_output() = outpt;
  std::string serialized_metadata;
  response.SerializeToString(&serialized_metadata);
  optimization_guide::proto::Any any;
  any.set_value(serialized_metadata);
  any.set_type_url("type.googleapis.com/" + response.GetTypeName());
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

void CheckComposeRequestRewriteParamsPreviousResponse(
    const google::protobuf::MessageLite& request_metadata,
    const std::string& previous_response) {
  const optimization_guide::proto::ComposeRequest* request =
      static_cast<const optimization_guide::proto::ComposeRequest*>(
          &request_metadata);
  EXPECT_THAT(request->rewrite_params().previous_response(), previous_response);
}

void CheckComposeRequestRewriteParamsTone(
    const google::protobuf::MessageLite& request_metadata,
    optimization_guide::proto::ComposeTone tone) {
  const optimization_guide::proto::ComposeRequest* request =
      static_cast<const optimization_guide::proto::ComposeRequest*>(
          &request_metadata);
  EXPECT_EQ(request->rewrite_params().tone(), tone);
}

void CheckComposeRequestRewriteParamsLength(
    const google::protobuf::MessageLite& request_metadata,
    optimization_guide::proto::ComposeLength length) {
  const optimization_guide::proto::ComposeRequest* request =
      static_cast<const optimization_guide::proto::ComposeRequest*>(
          &request_metadata);
  EXPECT_EQ(request->rewrite_params().length(), length);
}

void CheckComposeRequestRewriteParamsRegenerateFlag(
    const google::protobuf::MessageLite& request_metadata) {
  const optimization_guide::proto::ComposeRequest* request =
      static_cast<const optimization_guide::proto::ComposeRequest*>(
          &request_metadata);
  EXPECT_TRUE(request->rewrite_params().regenerate());
}

}  // namespace

class AIRewriterTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    mock_host_ = std::make_unique<MockSupportsUserData>();
  }

  void TearDown() override {
    optimization_guide_keyed_service_ = nullptr;
    mock_host_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  // Setting up MockOptimizationGuideKeyedService.
  void SetupMockOptimizationGuideKeyedService() {
    optimization_guide_keyed_service_ =
        static_cast<MockOptimizationGuideKeyedService*>(
            OptimizationGuideKeyedServiceFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    profile(),
                    base::BindRepeating([](content::BrowserContext* context)
                                            -> std::unique_ptr<KeyedService> {
                      return std::make_unique<
                          NiceMock<MockOptimizationGuideKeyedService>>();
                    })));
  }
  void SetupNullOptimizationGuideKeyedService() {
    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating(
                [](content::BrowserContext* context)
                    -> std::unique_ptr<KeyedService> { return nullptr; }));
  }

  mojo::Remote<blink::mojom::AIManager> GetAIManagerRemote() {
    AIManagerKeyedService* ai_manager_keyed_service =
        AIManagerKeyedServiceFactory::GetAIManagerKeyedService(
            main_rfh()->GetBrowserContext());
    mojo::Remote<blink::mojom::AIManager> ai_manager;
    ai_manager_keyed_service->AddReceiver(
        ai_manager.BindNewPipeAndPassReceiver(), mock_host_.get());
    return ai_manager;
  }
  void RunSimpleRewriteTest(
      blink::mojom::AIRewriterTone tone,
      blink::mojom::AIRewriterLength length,
      base::OnceCallback<void(const google::protobuf::MessageLite&
                                  request_metadata)> request_check_callback);
  void RunRewriteOptionCombinationFailureTest(
      blink::mojom::AIRewriterTone tone,
      blink::mojom::AIRewriterLength length);

  raw_ptr<MockOptimizationGuideKeyedService> optimization_guide_keyed_service_;

 private:
  std::unique_ptr<MockSupportsUserData> mock_host_;
};

void AIRewriterTest::RunSimpleRewriteTest(
    blink::mojom::AIRewriterTone tone,
    blink::mojom::AIRewriterLength length,
    base::OnceCallback<void(const google::protobuf::MessageLite&
                                request_metadata)> request_check_callback) {
  SetupMockOptimizationGuideKeyedService();
  EXPECT_CALL(*optimization_guide_keyed_service_, StartSession(_, _))
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
                  CheckComposeRequestRewriteParamsPreviousResponse(
                      request_metadata, kInputString);
                  std::move(request_check_callback).Run(request_metadata);
                  callback.Run(CreateExecutionResult("Result text",
                                                     /*is_complete=*/true));
                }));
        return session;
      }));

  mojo::Remote<blink::mojom::AIRewriter> rewriter_remote;
  {
    MockCreateRewriterClient mock_create_write_client;
    base::RunLoop run_roop;
    EXPECT_CALL(mock_create_write_client, OnResult(_))
        .WillOnce(testing::Invoke(
            [&](mojo::PendingRemote<::blink::mojom::AIRewriter> writer) {
              EXPECT_TRUE(writer);
              rewriter_remote =
                  mojo::Remote<blink::mojom::AIRewriter>(std::move(writer));
              run_roop.Quit();
            }));

    mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
    ai_manager->CreateRewriter(
        kSharedContextString, tone, length,
        mock_create_write_client.BindNewPipeAndPassRemote());
    run_roop.Run();
  }
  MockResponder mock_responder;

  base::RunLoop run_roop;
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
            run_roop.Quit();
          }));

  rewriter_remote->Rewrite(kInputString, kContextString,
                           mock_responder.BindNewPipeAndPassRemote());
  run_roop.Run();
}

void AIRewriterTest::RunRewriteOptionCombinationFailureTest(
    blink::mojom::AIRewriterTone tone,
    blink::mojom::AIRewriterLength length) {
  MockCreateRewriterClient mock_create_write_client;
  base::RunLoop run_roop;
  EXPECT_CALL(mock_create_write_client, OnResult(_))
      .WillOnce(testing::Invoke(
          [&](mojo::PendingRemote<::blink::mojom::AIRewriter> writer) {
            EXPECT_FALSE(writer);
            run_roop.Quit();
          }));

  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateRewriter(
      kSharedContextString, tone, length,
      mock_create_write_client.BindNewPipeAndPassRemote());
  run_roop.Run();
}

TEST_F(AIRewriterTest, CreateRewriterNoService) {
  SetupNullOptimizationGuideKeyedService();

  MockCreateRewriterClient mock_create_write_client;
  base::RunLoop run_roop;
  EXPECT_CALL(mock_create_write_client, OnResult(_))
      .WillOnce(testing::Invoke(
          [&](mojo::PendingRemote<::blink::mojom::AIRewriter> writer) {
            EXPECT_FALSE(writer);
            run_roop.Quit();
          }));

  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateRewriter(
      kSharedContextString, blink::mojom::AIRewriterTone::kAsIs,
      blink::mojom::AIRewriterLength::kAsIs,
      mock_create_write_client.BindNewPipeAndPassRemote());
  run_roop.Run();
}

TEST_F(AIRewriterTest, CreateRewriterStartSessionError) {
  SetupMockOptimizationGuideKeyedService();
  EXPECT_CALL(*optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) { return nullptr; }));

  MockCreateRewriterClient mock_create_write_client;
  base::RunLoop run_roop;
  EXPECT_CALL(mock_create_write_client, OnResult(_))
      .WillOnce(testing::Invoke(
          [&](mojo::PendingRemote<::blink::mojom::AIRewriter> writer) {
            EXPECT_FALSE(writer);
            run_roop.Quit();
          }));

  mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
  ai_manager->CreateRewriter(
      kSharedContextString, blink::mojom::AIRewriterTone::kAsIs,
      blink::mojom::AIRewriterLength::kAsIs,
      mock_create_write_client.BindNewPipeAndPassRemote());
  run_roop.Run();
}

TEST_F(AIRewriterTest, RewriteRegenerate) {
  RunSimpleRewriteTest(
      blink::mojom::AIRewriterTone::kAsIs,
      blink::mojom::AIRewriterLength::kAsIs,
      base::BindLambdaForTesting(
          [&](const google::protobuf::MessageLite& request_metadata) {
            CheckComposeRequestRewriteParamsRegenerateFlag(request_metadata);
          }));
}

TEST_F(AIRewriterTest, RewriteMoreCasual) {
  RunSimpleRewriteTest(
      blink::mojom::AIRewriterTone::kMoreCasual,
      blink::mojom::AIRewriterLength::kAsIs,
      base::BindLambdaForTesting(
          [&](const google::protobuf::MessageLite& request_metadata) {
            CheckComposeRequestRewriteParamsTone(
                request_metadata,
                optimization_guide::proto::ComposeTone::COMPOSE_INFORMAL);
          }));
}

TEST_F(AIRewriterTest, RewriteMoreFormal) {
  RunSimpleRewriteTest(
      blink::mojom::AIRewriterTone::kMoreFormal,
      blink::mojom::AIRewriterLength::kAsIs,
      base::BindLambdaForTesting(
          [&](const google::protobuf::MessageLite& request_metadata) {
            CheckComposeRequestRewriteParamsTone(
                request_metadata,
                optimization_guide::proto::ComposeTone::COMPOSE_FORMAL);
          }));
}

TEST_F(AIRewriterTest, RewriteLonger) {
  RunSimpleRewriteTest(
      blink::mojom::AIRewriterTone::kAsIs,
      blink::mojom::AIRewriterLength::kLonger,
      base::BindLambdaForTesting(
          [&](const google::protobuf::MessageLite& request_metadata) {
            CheckComposeRequestRewriteParamsLength(
                request_metadata,
                optimization_guide::proto::ComposeLength::COMPOSE_LONGER);
          }));
}

TEST_F(AIRewriterTest, RewriteShorter) {
  RunSimpleRewriteTest(
      blink::mojom::AIRewriterTone::kAsIs,
      blink::mojom::AIRewriterLength::kShorter,
      base::BindLambdaForTesting(
          [&](const google::protobuf::MessageLite& request_metadata) {
            CheckComposeRequestRewriteParamsLength(
                request_metadata,
                optimization_guide::proto::ComposeLength::COMPOSE_SHORTER);
          }));
}

TEST_F(AIRewriterTest, RewriteOptionCombinationFailureTest) {
  SetupMockOptimizationGuideKeyedService();
  struct {
    blink::mojom::AIRewriterTone tone;
    blink::mojom::AIRewriterLength length;
  } test_cases[]{
      {blink::mojom::AIRewriterTone::kMoreCasual,
       blink::mojom::AIRewriterLength::kLonger},
      {blink::mojom::AIRewriterTone::kMoreCasual,
       blink::mojom::AIRewriterLength::kShorter},
      {blink::mojom::AIRewriterTone::kMoreFormal,
       blink::mojom::AIRewriterLength::kLonger},
      {blink::mojom::AIRewriterTone::kMoreFormal,
       blink::mojom::AIRewriterLength::kShorter},
  };
  for (const auto& test_case : test_cases) {
    RunRewriteOptionCombinationFailureTest(test_case.tone, test_case.length);
  }
}

TEST_F(AIRewriterTest, RewriteError) {
  SetupMockOptimizationGuideKeyedService();
  EXPECT_CALL(*optimization_guide_keyed_service_, StartSession(_, _))
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
                  CheckComposeRequestRewriteParamsPreviousResponse(
                      request_metadata, kInputString);
                  callback.Run(CreateExecutionErrorResult(
                      optimization_guide::OptimizationGuideModelExecutionError::
                          FromModelExecutionError(
                              optimization_guide::
                                  OptimizationGuideModelExecutionError::
                                      ModelExecutionError::kPermissionDenied)));
                }));
        return session;
      }));

  mojo::Remote<blink::mojom::AIRewriter> rewriter_remote;
  {
    MockCreateRewriterClient mock_create_write_client;
    base::RunLoop run_roop;
    EXPECT_CALL(mock_create_write_client, OnResult(_))
        .WillOnce(testing::Invoke(
            [&](mojo::PendingRemote<::blink::mojom::AIRewriter> writer) {
              EXPECT_TRUE(writer);
              rewriter_remote =
                  mojo::Remote<blink::mojom::AIRewriter>(std::move(writer));
              run_roop.Quit();
            }));

    mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
    ai_manager->CreateRewriter(
        kSharedContextString, blink::mojom::AIRewriterTone::kAsIs,
        blink::mojom::AIRewriterLength::kAsIs,
        mock_create_write_client.BindNewPipeAndPassRemote());
    run_roop.Run();
  }
  MockResponder mock_responder;

  base::RunLoop run_roop;
  EXPECT_CALL(mock_responder, OnResponse(_, _, _))
      .WillOnce(testing::Invoke([&](blink::mojom::ModelStreamingResponseStatus
                                        status,
                                    const std::optional<std::string>& text,
                                    std::optional<uint64_t> current_tokens) {
        EXPECT_EQ(
            status,
            blink::mojom::ModelStreamingResponseStatus::kErrorPermissionDenied);
        run_roop.Quit();
      }));

  rewriter_remote->Rewrite(kInputString, kContextString,
                           mock_responder.BindNewPipeAndPassRemote());
  run_roop.Run();
}

TEST_F(AIRewriterTest, RewriteMultipleResponse) {
  SetupMockOptimizationGuideKeyedService();
  EXPECT_CALL(*optimization_guide_keyed_service_, StartSession(_, _))
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
                  CheckComposeRequestRewriteParamsPreviousResponse(
                      request_metadata, kInputString);

                  callback.Run(
                      CreateExecutionResult("Result ", /*is_complete=*/false));
                  callback.Run(
                      CreateExecutionResult("text", /*is_complete=*/true));
                }));
        return session;
      }));

  mojo::Remote<blink::mojom::AIRewriter> rewriter_remote;
  {
    MockCreateRewriterClient mock_create_write_client;
    base::RunLoop run_roop;
    EXPECT_CALL(mock_create_write_client, OnResult(_))
        .WillOnce(testing::Invoke(
            [&](mojo::PendingRemote<::blink::mojom::AIRewriter> writer) {
              EXPECT_TRUE(writer);
              rewriter_remote =
                  mojo::Remote<blink::mojom::AIRewriter>(std::move(writer));
              run_roop.Quit();
            }));

    mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
    ai_manager->CreateRewriter(
        kSharedContextString, blink::mojom::AIRewriterTone::kAsIs,
        blink::mojom::AIRewriterLength::kAsIs,
        mock_create_write_client.BindNewPipeAndPassRemote());
    run_roop.Run();
  }
  MockResponder mock_responder;

  base::RunLoop run_roop;
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
            run_roop.Quit();
          }));

  rewriter_remote->Rewrite(kInputString, kContextString,
                           mock_responder.BindNewPipeAndPassRemote());
  run_roop.Run();
}

TEST_F(AIRewriterTest, MultipleRewrite) {
  SetupMockOptimizationGuideKeyedService();
  EXPECT_CALL(*optimization_guide_keyed_service_, StartSession(_, _))
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
                  CheckComposeRequestRewriteParamsPreviousResponse(
                      request_metadata, kInputString);
                  callback.Run(CreateExecutionResult("Result text",
                                                     /*is_complete=*/true));
                }))
            .WillOnce(testing::Invoke(
                [](const google::protobuf::MessageLite& request_metadata,
                   optimization_guide::
                       OptimizationGuideModelExecutionResultStreamingCallback
                           callback) {
                  CheckComposeRequestRewriteParamsPreviousResponse(
                      request_metadata, "input string 2");
                  callback.Run(CreateExecutionResult("Result text 2",
                                                     /*is_complete=*/true));
                }));
        return session;
      }));

  mojo::Remote<blink::mojom::AIRewriter> rewriter_remote;
  {
    MockCreateRewriterClient mock_create_write_client;
    base::RunLoop run_roop;
    EXPECT_CALL(mock_create_write_client, OnResult(_))
        .WillOnce(testing::Invoke(
            [&](mojo::PendingRemote<::blink::mojom::AIRewriter> writer) {
              EXPECT_TRUE(writer);
              rewriter_remote =
                  mojo::Remote<blink::mojom::AIRewriter>(std::move(writer));
              run_roop.Quit();
            }));

    mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
    ai_manager->CreateRewriter(
        kSharedContextString, blink::mojom::AIRewriterTone::kAsIs,
        blink::mojom::AIRewriterLength::kAsIs,
        mock_create_write_client.BindNewPipeAndPassRemote());
    run_roop.Run();
  }
  {
    MockResponder mock_responder;
    base::RunLoop run_roop;
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
              run_roop.Quit();
            }));

    rewriter_remote->Rewrite(kInputString, kContextString,
                             mock_responder.BindNewPipeAndPassRemote());
    run_roop.Run();
  }
  {
    MockResponder mock_responder;
    base::RunLoop run_roop;
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
              run_roop.Quit();
            }));

    rewriter_remote->Rewrite("input string 2", "test context 2",
                             mock_responder.BindNewPipeAndPassRemote());
    run_roop.Run();
  }
}

TEST_F(AIRewriterTest, ResponderDisconnected) {
  SetupMockOptimizationGuideKeyedService();

  base::RunLoop run_roop_for_callback;
  optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
      streaming_callback;
  EXPECT_CALL(*optimization_guide_keyed_service_, StartSession(_, _))
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
                  CheckComposeRequestRewriteParamsPreviousResponse(
                      request_metadata, kInputString);
                  streaming_callback = std::move(callback);
                  run_roop_for_callback.Quit();
                }));
        return session;
      }));

  mojo::Remote<blink::mojom::AIRewriter> rewriter_remote;
  {
    MockCreateRewriterClient mock_create_write_client;
    base::RunLoop run_roop;
    EXPECT_CALL(mock_create_write_client, OnResult(_))
        .WillOnce(testing::Invoke(
            [&](mojo::PendingRemote<::blink::mojom::AIRewriter> writer) {
              EXPECT_TRUE(writer);
              rewriter_remote =
                  mojo::Remote<blink::mojom::AIRewriter>(std::move(writer));
              run_roop.Quit();
            }));

    mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
    ai_manager->CreateRewriter(
        kSharedContextString, blink::mojom::AIRewriterTone::kAsIs,
        blink::mojom::AIRewriterLength::kAsIs,
        mock_create_write_client.BindNewPipeAndPassRemote());
    run_roop.Run();
  }
  std::unique_ptr<MockResponder> mock_responder =
      std::make_unique<MockResponder>();
  rewriter_remote->Rewrite(kInputString, kContextString,
                           mock_responder->BindNewPipeAndPassRemote());
  mock_responder.reset();
  // Call RunUntilIdle() to disconnect the ModelStreamingResponder mojo remote
  // interface in AIRewriter.
  task_environment()->RunUntilIdle();

  run_roop_for_callback.Run();
  ASSERT_TRUE(streaming_callback);
  streaming_callback.Run(CreateExecutionResult("Result text",
                                               /*is_complete=*/true));
  task_environment()->RunUntilIdle();
}

TEST_F(AIRewriterTest, RewriterDisconnected) {
  SetupMockOptimizationGuideKeyedService();

  base::RunLoop run_roop_for_callback;
  optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
      streaming_callback;
  EXPECT_CALL(*optimization_guide_keyed_service_, StartSession(_, _))
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
                  CheckComposeRequestRewriteParamsPreviousResponse(
                      request_metadata, kInputString);
                  streaming_callback = std::move(callback);
                  run_roop_for_callback.Quit();
                }));
        return session;
      }));

  mojo::Remote<blink::mojom::AIRewriter> rewriter_remote;
  {
    MockCreateRewriterClient mock_create_write_client;
    base::RunLoop run_roop;
    EXPECT_CALL(mock_create_write_client, OnResult(_))
        .WillOnce(testing::Invoke(
            [&](mojo::PendingRemote<::blink::mojom::AIRewriter> writer) {
              EXPECT_TRUE(writer);
              rewriter_remote =
                  mojo::Remote<blink::mojom::AIRewriter>(std::move(writer));
              run_roop.Quit();
            }));

    mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
    ai_manager->CreateRewriter(
        kSharedContextString, blink::mojom::AIRewriterTone::kAsIs,
        blink::mojom::AIRewriterLength::kAsIs,
        mock_create_write_client.BindNewPipeAndPassRemote());
    run_roop.Run();
  }

  MockResponder mock_responder;
  base::RunLoop run_roop_for_response;
  EXPECT_CALL(mock_responder, OnResponse(_, _, _))
      .WillOnce(testing::Invoke([&](blink::mojom::ModelStreamingResponseStatus
                                        status,
                                    const std::optional<std::string>& text,
                                    std::optional<uint64_t> current_tokens) {
        // The OnResponse must be called with kErrorSessionDestroyed.
        EXPECT_EQ(
            status,
            blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
        run_roop_for_response.Quit();
      }));

  rewriter_remote->Rewrite(kInputString, kContextString,
                           mock_responder.BindNewPipeAndPassRemote());

  run_roop_for_callback.Run();

  // Disconnect the rewriter handle.
  rewriter_remote.reset();

  // Call RunUntilIdle() to destroy AIRewriter.
  task_environment()->RunUntilIdle();

  ASSERT_TRUE(streaming_callback);
  streaming_callback.Run(CreateExecutionResult("Result text",
                                               /*is_complete=*/true));
  run_roop_for_response.Run();
}

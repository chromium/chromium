// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_summarizer.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/ai/ai_manager_keyed_service.h"
#include "chrome/browser/ai/ai_manager_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/summarize.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

using ::testing::_;
using ::testing::AtMost;
using ::testing::NiceMock;

namespace {

using optimization_guide::MockSession;
using optimization_guide::MockSessionWrapper;

class MockSupportsUserData : public base::SupportsUserData {};

class MockStreamingResponder : public blink::mojom::ModelStreamingResponder {
 public:
  MockStreamingResponder() = default;
  ~MockStreamingResponder() override = default;
  MockStreamingResponder(const MockStreamingResponder&) = delete;
  MockStreamingResponder& operator=(const MockStreamingResponder&) = delete;

  void OnResponse(blink::mojom::ModelStreamingResponseStatus status,
                  const std::optional<std::string>& text,
                  const std::optional<uint64_t> current_tokens) override {
    status_ = status;

    if (text.has_value()) {
      result_ += text.value();
    }
    if (status_ != blink::mojom::ModelStreamingResponseStatus::kOngoing) {
      run_loop_.Quit();
    }
  }

  mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
  BindNewPipeAndPassRemote() {
    return responder_.BindNewPipeAndPassRemote();
  }

  blink::mojom::ModelStreamingResponseStatus status() { return status_; }

  std::string result() { return result_; }

  void WaitForResponseComplete() { run_loop_.Run(); }

 private:
  mojo::Receiver<blink::mojom::ModelStreamingResponder> responder_{this};

  blink::mojom::ModelStreamingResponseStatus status_;
  std::string result_;
  base::RunLoop run_loop_;
};

class AISummarizerUnitTest : public ChromeRenderViewHostTestHarness {
 public:
  AISummarizerUnitTest() = default;

  void SetUp() override { ChromeRenderViewHostTestHarness::SetUp(); }

  void TearDown() override {
    mock_optimization_guide_keyed_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetupMockOptimizationGuideKeyedService() {
    mock_optimization_guide_keyed_service_ =
        static_cast<NiceMock<MockOptimizationGuideKeyedService>*>(
            OptimizationGuideKeyedServiceFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    profile(),
                    base::BindRepeating([](content::BrowserContext* context)
                                            -> std::unique_ptr<KeyedService> {
                      return std::make_unique<
                          NiceMock<MockOptimizationGuideKeyedService>>();
                    })));

    ON_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
        .WillByDefault(
            [&] { return std::make_unique<MockSessionWrapper>(&session_); });
  }

  void SetupNullOptimizationGuideKeyedService() {
    mock_optimization_guide_keyed_service_ =
        static_cast<NiceMock<MockOptimizationGuideKeyedService>*>(
            OptimizationGuideKeyedServiceFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    profile(),
                    base::BindRepeating([](content::BrowserContext* context)
                                            -> std::unique_ptr<KeyedService> {
                      return nullptr;
                    })));
  }

  ~AISummarizerUnitTest() override = default;

 protected:
  MockSupportsUserData* mock_host() { return &mock_host_; }

  raw_ptr<MockOptimizationGuideKeyedService>
      mock_optimization_guide_keyed_service_;

 private:
  testing::NiceMock<MockSession> session_;
  MockSupportsUserData mock_host_;
};

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
    return responder_.BindNewPipeAndPassRemote();
  }

  void OnResult(
      mojo::PendingRemote<blink::mojom::AISummarizer> pending_remote) override {
    if (pending_remote) {
      summarizer_.Bind(std::move(pending_remote));
    }
    run_loop_.Quit();
  }

  mojo::Remote<blink::mojom::AISummarizer> summarizer() {
    return std::move(summarizer_);
  }

  void WaitForResult() { run_loop_.Run(); }

 private:
  mojo::Receiver<blink::mojom::AIManagerCreateSummarizerClient> responder_{
      this};
  mojo::Remote<blink::mojom::AISummarizer> summarizer_;

  base::RunLoop run_loop_;
};

}  // namespace

TEST_F(AISummarizerUnitTest, CreateSummarizerWithoutService) {
  base::RunLoop run_loop;

  SetupNullOptimizationGuideKeyedService();
  AIManagerKeyedService* ai_manager =
      AIManagerKeyedServiceFactory::GetAIManagerKeyedService(
          main_rfh()->GetBrowserContext());
  base::MockCallback<blink::mojom::AIManager::CanCreateSummarizerCallback>
      callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(AtMost(1))
      .WillOnce(testing::Invoke([&](blink::mojom::ModelAvailabilityCheckResult
                                        result) {
        EXPECT_EQ(
            result,
            blink::mojom::ModelAvailabilityCheckResult::kNoServiceNotRunning);
        run_loop.Quit();
      }));
  ai_manager->CanCreateSummarizer(callback.Get());
  run_loop.Run();

  // The callback may still be pending, delete the WebContents and destroy the
  // associated RFH, which should not result in a UAF.
  DeleteContents();
  task_environment()->RunUntilIdle();
}

TEST_F(AISummarizerUnitTest, SummarizeSuccess) {
  SetupMockOptimizationGuideKeyedService();
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              StartSession(testing::_, testing::_))
      .WillOnce(testing::Invoke([&](optimization_guide::ModelBasedCapabilityKey
                                        feature,
                                    const std::optional<
                                        optimization_guide::
                                            SessionConfigParams>&
                                        config_params) {
        auto session = std::make_unique<MockSession>();
        EXPECT_CALL(*session, ExecuteModel(testing::_, testing::_))
            .WillOnce(testing::Invoke(
                [&](const google::protobuf::MessageLite& request_metadata,
                    optimization_guide::
                        OptimizationGuideModelExecutionResultStreamingCallback
                            callback) {
                  optimization_guide::proto::SummarizeRequest request;
                  EXPECT_EQ(request.GetTypeName(),
                            request_metadata.GetTypeName());
                  request = static_cast<
                      const optimization_guide::proto::SummarizeRequest&>(
                      request_metadata);
                  EXPECT_EQ(request.article(), "Test input");
                  optimization_guide::proto::StringValue summary_str;
                  summary_str.set_value("Test output");
                  std::string serialized_metadata;
                  summary_str.SerializeToString(&serialized_metadata);
                  optimization_guide::proto::Any any;
                  any.set_value(serialized_metadata);
                  any.set_type_url("type.googleapis.com/" +
                                   summary_str.GetTypeName());
                  callback.Run(
                      optimization_guide::
                          OptimizationGuideModelStreamingExecutionResult(
                              optimization_guide::StreamingResponse{
                                  .response = any,
                                  .is_complete = true,
                              },
                              /*provided_by_on_device=*/true));
                }));
        return session;
      }));

  AIManagerKeyedService* ai_manager =
      AIManagerKeyedServiceFactory::GetAIManagerKeyedService(
          main_rfh()->GetBrowserContext());
  base::WeakPtr<AIContextBoundObjectSet> context_bound_objects =
      AIContextBoundObjectSet::GetFromContext(mock_host())
          ->GetWeakPtrForTesting();
  ASSERT_EQ(0u, context_bound_objects->GetSizeForTesting());

  mojo::Remote<blink::mojom::AIManager> mock_remote;
  ai_manager->AddReceiver(mock_remote.BindNewPipeAndPassReceiver(),
                          mock_host());
  MockCreateSummarizerClient create_client;
  mock_remote->CreateSummarizer(create_client.BindNewPipeAndPassRemote());
  create_client.WaitForResult();
  mojo::Remote<blink::mojom::AISummarizer> summarizer =
      create_client.summarizer();
  EXPECT_TRUE(summarizer);
  ASSERT_EQ(1u, context_bound_objects->GetSizeForTesting());

  MockStreamingResponder responder;
  summarizer->Summarize("Test input", responder.BindNewPipeAndPassRemote());
  responder.WaitForResponseComplete();
  EXPECT_EQ(responder.status(),
            blink::mojom::ModelStreamingResponseStatus::kComplete);
  EXPECT_EQ(responder.result(), "Test output");

  summarizer.reset();
  task_environment()->RunUntilIdle();
  ASSERT_FALSE(context_bound_objects);
}

TEST_F(AISummarizerUnitTest, SessionDetachedDuringSummarization) {
  // The ExecuteModel behavior is not overridden so the responder will
  // not receive anything. The test will detach the session while waiting
  // for the response.
  SetupMockOptimizationGuideKeyedService();
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              StartSession(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) { return std::make_unique<MockSession>(); }));

  AIManagerKeyedService* ai_manager =
      AIManagerKeyedServiceFactory::GetAIManagerKeyedService(
          main_rfh()->GetBrowserContext());
  base::WeakPtr<AIContextBoundObjectSet> context_bound_objects =
      AIContextBoundObjectSet::GetFromContext(mock_host())
          ->GetWeakPtrForTesting();
  ASSERT_EQ(0u, context_bound_objects->GetSizeForTesting());

  mojo::Remote<blink::mojom::AIManager> mock_remote;
  ai_manager->AddReceiver(mock_remote.BindNewPipeAndPassReceiver(),
                          mock_host());
  MockCreateSummarizerClient create_client;
  mock_remote->CreateSummarizer(create_client.BindNewPipeAndPassRemote());
  create_client.WaitForResult();
  mojo::Remote<blink::mojom::AISummarizer> summarizer =
      create_client.summarizer();
  EXPECT_TRUE(summarizer);
  ASSERT_EQ(1u, context_bound_objects->GetSizeForTesting());

  MockStreamingResponder responder;
  summarizer->Summarize("Test input", responder.BindNewPipeAndPassRemote());

  summarizer.reset();
  task_environment()->RunUntilIdle();
  ASSERT_FALSE(context_bound_objects);
}

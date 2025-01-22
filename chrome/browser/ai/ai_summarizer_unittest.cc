// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_summarizer.h"

#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "chrome/browser/ai/ai_manager.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/summarize.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

using ::testing::_;
using ::testing::AtMost;
using ::testing::NiceMock;

namespace {

using optimization_guide::MockSession;
using optimization_guide::proto::SummarizeRequest;
using optimization_guide::proto::SummarizerOutputFormat;
using optimization_guide::proto::SummarizerOutputLength;
using optimization_guide::proto::SummarizerOutputType;

class AISummarizerUnitTest : public AITestUtils::AITestBase {
 public:
  AISummarizerUnitTest() = default;

  void SetupMockOptimizationGuideKeyedService() override {
    AITestUtils::AITestBase::SetupMockOptimizationGuideKeyedService();

    ON_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
        .WillByDefault(
            [&] { return std::make_unique<NiceMock<MockSession>>(&session_); });
  }

  ~AISummarizerUnitTest() override = default;

 protected:
  testing::NiceMock<MockSession> session_;
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
  AIManager ai_manager = AIManager(main_rfh()->GetBrowserContext());
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
  ai_manager.CanCreateSummarizer(callback.Get());
  run_loop.Run();

  // The callback may still be pending, delete the WebContents and destroy the
  // associated RFH, which should not result in a UAF.
  DeleteContents();
  task_environment()->RunUntilIdle();
}

testing::Action<void(
    const google::protobuf::MessageLite&,
    optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback)>
CreateModelExecutionMock(const std::string& expected_input,
                         const std::string& expected_context,
                         SummarizerOutputType expected_output_type,
                         SummarizerOutputFormat expected_output_format,
                         SummarizerOutputLength expected_output_length,
                         const std::string& output) {
  return
      [=](const google::protobuf::MessageLite& request_metadata,
          optimization_guide::
              OptimizationGuideModelExecutionResultStreamingCallback callback) {
        optimization_guide::proto::SummarizeRequest request;
        EXPECT_EQ(request.GetTypeName(), request_metadata.GetTypeName());
        request =
            static_cast<const optimization_guide::proto::SummarizeRequest&>(
                request_metadata);
        EXPECT_EQ(request.article(), expected_input);
        EXPECT_EQ(request.context(), expected_context);
        EXPECT_EQ(request.options().output_type(), expected_output_type);
        EXPECT_EQ(request.options().output_format(), expected_output_format);
        EXPECT_EQ(request.options().output_length(), expected_output_length);
        optimization_guide::proto::StringValue summary_str;
        summary_str.set_value(output);
        callback.Run(
            optimization_guide::OptimizationGuideModelStreamingExecutionResult(
                optimization_guide::StreamingResponse{
                    .response = optimization_guide::AnyWrapProto(summary_str),
                    .is_complete = true,
                },
                /*provided_by_on_device=*/true));
      };
}

TEST_F(AISummarizerUnitTest, SummarizeSuccess) {
  SetupMockOptimizationGuideKeyedService();
  EXPECT_CALL(session_, ExecuteModel(testing::_, testing::_))
      .WillOnce(CreateModelExecutionMock(
          "Test input", "", SummarizerOutputType::SUMMARIZER_OUTPUT_TYPE_TL_DR,
          SummarizerOutputFormat::SUMMARIZER_OUTPUT_FORMAT_PLAIN_TEXT,
          SummarizerOutputLength::SUMMARIZER_OUTPUT_LENGTH_MEDIUM,
          "Test output"));

  ASSERT_EQ(0u, GetAIManagerContextBoundObjectSetSize());

  mojo::Remote<blink::mojom::AIManager> mock_remote = GetAIManagerRemote();
  MockCreateSummarizerClient create_client;
  mock_remote->CreateSummarizer(
      create_client.BindNewPipeAndPassRemote(),
      blink::mojom::AISummarizerCreateOptions::New(
          /*shared_context=*/"", blink::mojom::AISummarizerType::kTLDR,
          blink::mojom::AISummarizerFormat::kPlainText,
          blink::mojom::AISummarizerLength::kMedium));
  create_client.WaitForResult();
  mojo::Remote<blink::mojom::AISummarizer> summarizer =
      create_client.summarizer();
  EXPECT_TRUE(summarizer);
  ASSERT_EQ(1u, GetAIManagerContextBoundObjectSetSize());

  AITestUtils::MockModelStreamingResponder mock_responder;

  base::RunLoop run_loop;
  EXPECT_CALL(mock_responder, OnStreaming(_, _))
      .WillOnce(testing::Invoke(
          [&](const std::string& text,
              blink::mojom::ModelStreamingResponderAction action) {
            EXPECT_THAT(text, "Test output");
            EXPECT_EQ(action,
                      blink::mojom::ModelStreamingResponderAction::kReplace);
          }));

  EXPECT_CALL(mock_responder, OnCompletion(_))
      .WillOnce(testing::Invoke(
          [&](blink::mojom::ModelExecutionContextInfoPtr context_info) {
            run_loop.Quit();
          }));

  summarizer->Summarize("Test input", "",
                        mock_responder.BindNewPipeAndPassRemote());
  run_loop.Run();

  summarizer.reset();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return GetAIManagerContextBoundObjectSetSize() == 0u; }));
}

TEST_F(AISummarizerUnitTest, SessionDetachedDuringSummarization) {
  // The ExecuteModel behavior is not overridden so the responder will
  // not receive anything. The test will detach the session while waiting
  // for the response.
  SetupMockOptimizationGuideKeyedService();

  ASSERT_EQ(0u, GetAIManagerContextBoundObjectSetSize());

  mojo::Remote<blink::mojom::AIManager> mock_remote = GetAIManagerRemote();
  MockCreateSummarizerClient create_client;
  mock_remote->CreateSummarizer(
      create_client.BindNewPipeAndPassRemote(),
      blink::mojom::AISummarizerCreateOptions::New(
          /*shared_context=*/"", blink::mojom::AISummarizerType::kTLDR,
          blink::mojom::AISummarizerFormat::kPlainText,
          blink::mojom::AISummarizerLength::kMedium));
  create_client.WaitForResult();
  mojo::Remote<blink::mojom::AISummarizer> summarizer =
      create_client.summarizer();
  EXPECT_TRUE(summarizer);
  ASSERT_EQ(1u, GetAIManagerContextBoundObjectSetSize());

  AITestUtils::MockModelStreamingResponder mock_responder;
  summarizer->Summarize("Test input", /*context=*/"",
                        mock_responder.BindNewPipeAndPassRemote());

  summarizer.reset();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return GetAIManagerContextBoundObjectSetSize() == 0u; }));
}

TEST_F(AISummarizerUnitTest, MultipleSummarizeWithOptions) {
  SetupMockOptimizationGuideKeyedService();

  ASSERT_EQ(0u, GetAIManagerContextBoundObjectSetSize());

  mojo::Remote<blink::mojom::AIManager> mock_remote = GetAIManagerRemote();
  EXPECT_CALL(session_, ExecuteModel(testing::_, testing::_))
      .WillOnce(CreateModelExecutionMock(
          "Test input1", "Shared context.\n",
          SummarizerOutputType::SUMMARIZER_OUTPUT_TYPE_TEASER,
          SummarizerOutputFormat::SUMMARIZER_OUTPUT_FORMAT_MARKDOWN,
          SummarizerOutputLength::SUMMARIZER_OUTPUT_LENGTH_LONG,
          "Test output1"));
  MockCreateSummarizerClient create_client;
  mock_remote->CreateSummarizer(
      create_client.BindNewPipeAndPassRemote(),
      blink::mojom::AISummarizerCreateOptions::New(
          "Shared context.", blink::mojom::AISummarizerType::kTeaser,
          blink::mojom::AISummarizerFormat::kMarkDown,
          blink::mojom::AISummarizerLength::kLong));
  create_client.WaitForResult();
  mojo::Remote<blink::mojom::AISummarizer> summarizer =
      create_client.summarizer();
  EXPECT_TRUE(summarizer);
  ASSERT_EQ(1u, GetAIManagerContextBoundObjectSetSize());

  {
    AITestUtils::MockModelStreamingResponder mock_responder;

    base::RunLoop run_loop;
    EXPECT_CALL(mock_responder, OnStreaming(_, _))
        .WillOnce(testing::Invoke(
            [&](const std::string& text,
                blink::mojom::ModelStreamingResponderAction action) {
              EXPECT_THAT(text, "Test output1");
              EXPECT_EQ(action,
                        blink::mojom::ModelStreamingResponderAction::kReplace);
            }));

    EXPECT_CALL(mock_responder, OnCompletion(_))
        .WillOnce(testing::Invoke(
            [&](blink::mojom::ModelExecutionContextInfoPtr context_info) {
              run_loop.Quit();
            }));

    summarizer->Summarize("Test input1", /*context=*/"",
                          mock_responder.BindNewPipeAndPassRemote());
    run_loop.Run();
  }

  EXPECT_CALL(session_, ExecuteModel(testing::_, testing::_))
      .WillOnce(CreateModelExecutionMock(
          "Test input2", "Shared context. New context.\n",
          SummarizerOutputType::SUMMARIZER_OUTPUT_TYPE_TEASER,
          SummarizerOutputFormat::SUMMARIZER_OUTPUT_FORMAT_MARKDOWN,
          SummarizerOutputLength::SUMMARIZER_OUTPUT_LENGTH_LONG,
          "Test output2"));
  {
    AITestUtils::MockModelStreamingResponder mock_responder;

    base::RunLoop run_loop;
    EXPECT_CALL(mock_responder, OnStreaming(_, _))
        .WillOnce(testing::Invoke(
            [&](const std::string& text,
                blink::mojom::ModelStreamingResponderAction action) {
              EXPECT_THAT(text, "Test output2");
              EXPECT_EQ(action,
                        blink::mojom::ModelStreamingResponderAction::kReplace);
            }));

    EXPECT_CALL(mock_responder, OnCompletion(_))
        .WillOnce(testing::Invoke(
            [&](blink::mojom::ModelExecutionContextInfoPtr context_info) {
              run_loop.Quit();
            }));

    summarizer->Summarize("Test input2", "New context.",
                          mock_responder.BindNewPipeAndPassRemote());
    run_loop.Run();
  }

  summarizer.reset();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return GetAIManagerContextBoundObjectSetSize() == 0u; }));
}

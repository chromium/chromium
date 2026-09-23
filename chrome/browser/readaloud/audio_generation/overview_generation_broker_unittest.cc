// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/audio_generation/overview_generation_broker.h"

#include <string>
#include <string_view>

#include "base/strings/string_view_util.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/common/readaloud/read_aloud_constants.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/read_aloud_generate_text.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace readaloud {

class OverviewGenerationBrokerTest : public ::testing::Test {
 protected:
  using GenerateTextResponse =
      optimization_guide::proto::ReadAloudGenerateTextResponse;
  using ResponseFuture =
      base::test::TestFuture<mojo_base::BigBuffer, bool>;

  content::BrowserTaskEnvironment task_environment_;
  OverviewGenerationBroker broker_;
};

TEST_F(OverviewGenerationBrokerTest, BuildGenerateTextRequestBasic) {
  optimization_guide::proto::ReadAloudGenerateTextRequest request =
      broker_.BuildGenerateTextRequest(/*page_title=*/"Page Title",
                                       /*page_content=*/"Page Content Here",
                                       /*page_url=*/GURL("https://example.com/article"),
                                       /*language_code=*/"en-US");

  EXPECT_EQ(request.page_title(), "Page Title");
  EXPECT_EQ(request.page_content(), "Page Content Here");
  EXPECT_EQ(request.page_url(), "https://example.com/article");
  EXPECT_EQ(request.language_code(), "en-US");
}

TEST_F(OverviewGenerationBrokerTest, BuildGenerateTextRequestUrlSanitization) {
  // Verifies query, ref fragments, and credentials (username/password) are removed.
  GURL dirty_url(
      "https://user:secret@example.com:8080/path/to/page?utm_source=test&id=42#section2");
  optimization_guide::proto::ReadAloudGenerateTextRequest request =
      broker_.BuildGenerateTextRequest(/*page_title=*/"Title",
                                       /*page_content=*/"Content",
                                       /*page_url=*/dirty_url,
                                       /*language_code=*/"en");

  EXPECT_EQ(request.page_url(), "https://example.com:8080/path/to/page");
}

TEST_F(OverviewGenerationBrokerTest, BuildGenerateTextRequestNonWebUrlIgnored) {
  // Non-HTTP/HTTPS schemes (such as file://, chrome://, data:) should be omitted.
  optimization_guide::proto::ReadAloudGenerateTextRequest request_file =
      broker_.BuildGenerateTextRequest(
          /*page_title=*/"Title", /*page_content=*/"Content",
          /*page_url=*/GURL("file:///usr/local/home/doc.html"),
          /*language_code=*/"en");
  EXPECT_THAT(request_file.page_url(), testing::IsEmpty());

  optimization_guide::proto::ReadAloudGenerateTextRequest request_chrome =
      broker_.BuildGenerateTextRequest(
          /*page_title=*/"Title", /*page_content=*/"Content",
          /*page_url=*/GURL("chrome://settings"), /*language_code=*/"en");
  EXPECT_THAT(request_chrome.page_url(), testing::IsEmpty());

  optimization_guide::proto::ReadAloudGenerateTextRequest request_data =
      broker_.BuildGenerateTextRequest(
          /*page_title=*/"Title", /*page_content=*/"Content",
          /*page_url=*/GURL("data:text/html,<h1>Test</h1>"),
          /*language_code=*/"en");
  EXPECT_THAT(request_data.page_url(), testing::IsEmpty());
}

TEST_F(OverviewGenerationBrokerTest, BuildGenerateTextRequestLanguageCode) {
  optimization_guide::proto::ReadAloudGenerateTextRequest request_us =
      broker_.BuildGenerateTextRequest(/*page_title=*/"Title",
                                       /*page_content=*/"Content",
                                       /*page_url=*/GURL("https://example.com"),
                                       /*language_code=*/"en-US");
  EXPECT_EQ(request_us.language_code(), "en-US");

  optimization_guide::proto::ReadAloudGenerateTextRequest request_es =
      broker_.BuildGenerateTextRequest(/*page_title=*/"Title",
                                       /*page_content=*/"Content",
                                       /*page_url=*/GURL("https://example.com"),
                                       /*language_code=*/"es");
  EXPECT_EQ(request_es.language_code(), "es");
}

TEST_F(OverviewGenerationBrokerTest, BuildGenerateTextRequestEmptyLanguageCode) {
  optimization_guide::proto::ReadAloudGenerateTextRequest request =
      broker_.BuildGenerateTextRequest(/*page_title=*/"Title",
                                       /*page_content=*/"Content",
                                       /*page_url=*/GURL("https://example.com"),
                                       /*language_code=*/"");
  EXPECT_THAT(request.language_code(), testing::IsEmpty());
}

TEST_F(OverviewGenerationBrokerTest,
       BuildGenerateTextRequestInvalidLanguageCode) {
  optimization_guide::proto::ReadAloudGenerateTextRequest request =
      broker_.BuildGenerateTextRequest(/*page_title=*/"Title",
                                       /*page_content=*/"Content",
                                       /*page_url=*/GURL("https://example.com"),
                                       /*language_code=*/"invalid_xyz_tag_123");
  EXPECT_THAT(request.language_code(), testing::IsEmpty());
}

TEST_F(OverviewGenerationBrokerTest, GenerateOverviewNullOptGuideService) {
  ResponseFuture future;
  broker_.GenerateOverview(/*opt_guide_service=*/nullptr,
                           /*page_title=*/"Title",
                           /*page_content=*/"Some valid content",
                           /*page_url=*/GURL("https://example.com"),
                           /*language_code=*/"en", future.GetCallback());
  auto [response_bytes, success] = future.Take();
  EXPECT_FALSE(success);
  EXPECT_EQ(response_bytes.size(), 0u);
}

TEST_F(OverviewGenerationBrokerTest, GenerateOverviewEmptyContent) {
  testing::NiceMock<MockOptimizationGuideKeyedService> mock_opt_guide;
  ResponseFuture future;
  broker_.GenerateOverview(&mock_opt_guide, /*page_title=*/"Title",
                           /*page_content=*/"",
                           /*page_url=*/GURL("https://example.com"),
                           /*language_code=*/"en", future.GetCallback());
  auto [response_bytes, success] = future.Take();
  EXPECT_FALSE(success);
  EXPECT_EQ(response_bytes.size(), 0u);
}

TEST_F(OverviewGenerationBrokerTest, GenerateOverviewSuccess) {
  testing::NiceMock<MockOptimizationGuideKeyedService> mock_opt_guide;

  GenerateTextResponse expected_proto;
  expected_proto.set_generated_text("Summary text");
  optimization_guide::proto::DialogueTurn* turn1 =
      expected_proto.add_dialogue_turns();
  turn1->set_speaker("speaker1");
  turn1->set_utterance("Welcome to the AI Overview.");
  optimization_guide::proto::DialogueTurn* turn2 =
      expected_proto.add_dialogue_turns();
  turn2->set_speaker("speaker2");
  turn2->set_utterance("Here are the key takeaways.");

  optimization_guide::proto::Any any_response =
      optimization_guide::AnyWrapProto(expected_proto);

  EXPECT_CALL(
      mock_opt_guide,
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kReadAloudGenerateText,
          testing::_, testing::_, testing::_))
      .WillOnce(
          [&any_response](
              optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            EXPECT_EQ(options.execution_timeout,
                      readaloud::kOverviewGenerationTimeout);

            std::move(callback).Run(
                optimization_guide::OptimizationGuideModelExecutionResult(
                    any_response, /*execution_info=*/nullptr),
                /*log_entry=*/nullptr);
          });

  ResponseFuture future;
  broker_.GenerateOverview(&mock_opt_guide, /*page_title=*/"Test Title",
                           /*page_content=*/"Test Content",
                           /*page_url=*/GURL("https://example.com/test"),
                           /*language_code=*/"en",
                           future.GetCallback());

  auto [response_bytes, success] = future.Take();
  EXPECT_TRUE(success);
  EXPECT_EQ(base::as_string_view(response_bytes),
            expected_proto.SerializeAsString());
}

TEST_F(OverviewGenerationBrokerTest, GenerateOverviewModelExecutionFailure) {
  testing::NiceMock<MockOptimizationGuideKeyedService> mock_opt_guide;

  EXPECT_CALL(
      mock_opt_guide,
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kReadAloudGenerateText,
          testing::_, testing::_, testing::_))
      .WillOnce([](optimization_guide::ModelBasedCapabilityKey feature,
                   const google::protobuf::MessageLite& request_metadata,
                   const optimization_guide::ModelExecutionOptions& options,
                   optimization_guide::
                       OptimizationGuideModelExecutionResultCallback callback) {
        std::move(callback).Run(
            optimization_guide::OptimizationGuideModelExecutionResult(
                base::unexpected(
                    optimization_guide::OptimizationGuideModelExecutionError::
                        FromModelExecutionError(
                            optimization_guide::
                                OptimizationGuideModelExecutionError::
                                    ModelExecutionError::kGenericFailure)),
                /*execution_info=*/nullptr),
            /*log_entry=*/nullptr);
      });

  ResponseFuture future;
  broker_.GenerateOverview(&mock_opt_guide, /*page_title=*/"Test Title",
                           /*page_content=*/"Test Content",
                           /*page_url=*/GURL("https://example.com/test"),
                           /*language_code=*/"en",
                           future.GetCallback());
  auto [response_bytes, success] = future.Take();
  EXPECT_FALSE(success);
  EXPECT_EQ(response_bytes.size(), 0u);
}

TEST_F(OverviewGenerationBrokerTest, InvalidatePendingRequestsCancelsCallback) {
  testing::NiceMock<MockOptimizationGuideKeyedService> mock_opt_guide;
  optimization_guide::OptimizationGuideModelExecutionResultCallback saved_callback;

  EXPECT_CALL(
      mock_opt_guide,
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kReadAloudGenerateText,
          testing::_, testing::_, testing::_))
      .WillOnce(
          [&saved_callback](
              optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            saved_callback = std::move(callback);
          });

  ResponseFuture future;
  broker_.GenerateOverview(&mock_opt_guide, /*page_title=*/"Title",
                           /*page_content=*/"Content",
                           /*page_url=*/GURL("https://example.com"),
                           /*language_code=*/"en",
                           future.GetCallback());

  ASSERT_TRUE(saved_callback);
  EXPECT_FALSE(future.IsReady());

  // Invalidate pending requests before the service resolves.
  broker_.InvalidatePendingRequests();

  GenerateTextResponse resp;
  std::move(saved_callback)
      .Run(optimization_guide::OptimizationGuideModelExecutionResult(
               optimization_guide::AnyWrapProto(resp),
               /*execution_info=*/nullptr),
           /*log_entry=*/nullptr);

  // A response is never received because the request was cancelled.
  EXPECT_FALSE(future.IsReady());
}

TEST_F(OverviewGenerationBrokerTest,
       OverlappingRequestCancelsPreviousInFlightCallback) {
  testing::NiceMock<MockOptimizationGuideKeyedService> mock_opt_guide;
  optimization_guide::OptimizationGuideModelExecutionResultCallback callback1;
  optimization_guide::OptimizationGuideModelExecutionResultCallback callback2;

  EXPECT_CALL(
      mock_opt_guide,
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kReadAloudGenerateText,
          testing::_, testing::_, testing::_))
      .WillOnce(
          [&callback1](
              optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  cb) {
            callback1 = std::move(cb);
          })
      .WillOnce(
          [&callback2](
              optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  cb) {
            callback2 = std::move(cb);
          });

  ResponseFuture future1;
  broker_.GenerateOverview(&mock_opt_guide, /*page_title=*/"Title 1",
                           /*page_content=*/"Content 1",
                           /*page_url=*/GURL("https://example.com/1"),
                           /*language_code=*/"en",
                           future1.GetCallback());
  ASSERT_TRUE(callback1);

  ResponseFuture future2;
  broker_.GenerateOverview(&mock_opt_guide, /*page_title=*/"Title 2",
                           /*page_content=*/"Content 2",
                           /*page_url=*/GURL("https://example.com/2"),
                           /*language_code=*/"en",
                           future2.GetCallback());
  ASSERT_TRUE(callback2);

  // Resolve callback 1; it should be ignored because request 2 invalidated it.
  GenerateTextResponse resp1;
  std::move(callback1).Run(
      optimization_guide::OptimizationGuideModelExecutionResult(
          optimization_guide::AnyWrapProto(resp1), /*execution_info=*/nullptr),
      /*log_entry=*/nullptr);
  EXPECT_FALSE(future1.IsReady());

  // Resolve callback 2; it should run successfully.
  GenerateTextResponse resp2;
  resp2.set_generated_text("Second response");
  std::move(callback2).Run(
      optimization_guide::OptimizationGuideModelExecutionResult(
          optimization_guide::AnyWrapProto(resp2), /*execution_info=*/nullptr),
      /*log_entry=*/nullptr);
  EXPECT_TRUE(future2.IsReady());
  auto [response_bytes, success] = future2.Take();
  EXPECT_TRUE(success);
  EXPECT_EQ(base::as_string_view(response_bytes), resp2.SerializeAsString());
}

}  // namespace readaloud

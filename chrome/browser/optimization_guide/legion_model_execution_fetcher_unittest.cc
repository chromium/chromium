// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/legion_model_execution_fetcher.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/legion/client.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/zero_state_suggestions.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class MockLegionClient : public legion::Client {
 public:
  MOCK_METHOD(void,
              EstablishSession,
              (OnEstablishSessionCompletedCallback callback),
              (override));
  MOCK_METHOD(void,
              SendTextRequest,
              (legion::proto::FeatureName feature_name,
               const std::string& text,
               OnTextRequestCompletedCallback callback,
               const RequestOptions& options),
              (override));
  MOCK_METHOD(void,
              SendGenerateContentRequest,
              (legion::proto::FeatureName feature_name,
               const legion::proto::GenerateContentRequest& request,
               OnGenerateContentRequestCompletedCallback callback,
               const RequestOptions& options),
              (override));
};

class LegionModelExecutionFetcherTest : public testing::Test {
 public:
  void SetUp() override {
    fetcher_ =
        std::make_unique<LegionModelExecutionFetcher>(&mock_legion_client_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  MockLegionClient mock_legion_client_;
  std::unique_ptr<LegionModelExecutionFetcher> fetcher_;
};

TEST_F(LegionModelExecutionFetcherTest, ConvertsZeroStateSuggestionsRequest) {
  proto::ZeroStateSuggestionsRequest request;
  request.mutable_page_context()->set_url("url");
  request.mutable_page_context()->set_title("Hello");

  const std::string expected_prompt =
      "Please provide 3 short suggestions for what you could ask Gemini\n"
      "about the content of the following list of websites.\n"
      "Please provide each suggestion on a separate line and no other\n"
      "content in your response. No more than 30 characters per\n"
      "suggestion.\n"
      "Websites:\n"
      "url - Hello\n";

  EXPECT_CALL(
      mock_legion_client_,
      SendTextRequest(
          testing::Eq(legion::proto::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION),
          testing::Eq(expected_prompt), testing::_,
          testing::Field(&legion::Client::RequestOptions::timeout,
                         legion::Client::kDefaultTimeout)))
      .WillOnce([](legion::proto::FeatureName feature_name,
                   const std::string& request,
                   legion::Client::OnTextRequestCompletedCallback callback,
                   const legion::Client::RequestOptions& options) {});

  fetcher_->ExecuteModel(ModelBasedCapabilityKey::kZeroStateSuggestions,
                         /*identity_manager=*/nullptr, request,
                         /*timeout=*/std::nullopt, base::DoNothing());
}
TEST_F(LegionModelExecutionFetcherTest,
       ConvertsZeroStateSuggestionsRequestWithLists) {
  proto::ZeroStateSuggestionsRequest request;
  auto* list = request.mutable_page_context_list();
  auto* context1 = list->add_page_contexts()->mutable_page_context();
  context1->set_url("url1");
  context1->set_title("Привіт");
  auto* context2 = list->add_page_contexts()->mutable_page_context();
  context2->set_url("url2");
  context2->set_title("你好");

  const std::string expected_prompt =
      "Please provide 3 short suggestions for what you could ask Gemini\n"
      "about the content of the following list of websites.\n"
      "Please provide each suggestion on a separate line and no other\n"
      "content in your response. No more than 30 characters per\n"
      "suggestion.\n"
      "Websites:\n"
      "url1 - Привіт\n"
      "url2 - 你好\n";

  EXPECT_CALL(
      mock_legion_client_,
      SendTextRequest(
          testing::Eq(legion::proto::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION),
          testing::Eq(expected_prompt), testing::_,
          testing::Field(&legion::Client::RequestOptions::timeout,
                         legion::Client::kDefaultTimeout)))
      .WillOnce([](legion::proto::FeatureName feature_name,
                   const std::string& request,
                   legion::Client::OnTextRequestCompletedCallback callback,
                   const legion::Client::RequestOptions& options) {});

  fetcher_->ExecuteModel(ModelBasedCapabilityKey::kZeroStateSuggestions,
                         /*identity_manager=*/nullptr, request,
                         /*timeout=*/std::nullopt, base::DoNothing());
}

TEST_F(LegionModelExecutionFetcherTest, ConvertsZeroStateSuggestionsResponse) {
  const std::string legion_response = "Hello\nПривіт\n你好";

  EXPECT_CALL(
      mock_legion_client_,
      SendTextRequest(
          testing::Eq(legion::proto::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION),
          testing::_, testing::_,
          testing::Field(&legion::Client::RequestOptions::timeout,
                         legion::Client::kDefaultTimeout)))
      .WillOnce([&](legion::proto::FeatureName feature_name,
                    const std::string& request,
                    legion::Client::OnTextRequestCompletedCallback callback,
                    const legion::Client::RequestOptions& options) {
        std::move(callback).Run(base::ok(legion_response));
      });

  base::test::TestFuture<base::expected<const proto::ExecuteResponse,
                                        OptimizationGuideModelExecutionError>>
      future;
  fetcher_->ExecuteModel(ModelBasedCapabilityKey::kZeroStateSuggestions,
                         /*identity_manager=*/nullptr,
                         proto::ZeroStateSuggestionsRequest(),
                         /*timeout=*/std::nullopt, future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.has_value());
  auto zss_response = optimization_guide::ParsedAnyMetadata<
      proto::ZeroStateSuggestionsResponse>(result.value().response_metadata());
  EXPECT_EQ(zss_response->suggestions(0).label(), "Hello");
  EXPECT_EQ(zss_response->suggestions(1).label(), "Привіт");
  EXPECT_EQ(zss_response->suggestions(2).label(), "你好");
}

}  // namespace optimization_guide

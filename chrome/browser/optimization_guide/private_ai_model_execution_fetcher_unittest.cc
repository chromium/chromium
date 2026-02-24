// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/private_ai_model_execution_fetcher.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/zero_state_suggestions.pb.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "components/private_ai/testing/mock_private_ai_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class PrivateAiModelExecutionFetcherTest : public testing::Test {
 public:
  void SetUp() override {
    fetcher_ = std::make_unique<PrivateAiModelExecutionFetcher>(
        &mock_private_ai_client_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  private_ai::MockPrivateAiClient mock_private_ai_client_;
  std::unique_ptr<PrivateAiModelExecutionFetcher> fetcher_;
};

TEST_F(PrivateAiModelExecutionFetcherTest,
       ConvertsZeroStateSuggestionsRequest) {
  proto::ZeroStateSuggestionsRequest request;
  request.mutable_page_context()->set_url("url");
  request.mutable_page_context()->set_title("Hello");

  EXPECT_CALL(
      mock_private_ai_client_,
      SendPaicRequest(
          testing::Eq(
              private_ai::proto::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION),
          testing::_, testing::_,
          testing::Field(&private_ai::Client::RequestOptions::timeout,
                         private_ai::Client::kDefaultTimeout)))
      .WillOnce(
          [](private_ai::proto::FeatureName feature_name,
             const private_ai::proto::PaicMessage& request,
             private_ai::Client::OnPaicMessageRequestCompletedCallback callback,
             const private_ai::Client::RequestOptions& options) {
            auto execute_request = request.execute_request_ext();
            auto zss_request =
                ParsedAnyMetadata<proto::ZeroStateSuggestionsRequest>(
                    execute_request.request_metadata());
            EXPECT_EQ(zss_request->page_context().url(), "url");
            EXPECT_EQ(zss_request->page_context().title(), "Hello");
            std::move(callback).Run(base::ok(private_ai::proto::PaicMessage()));
          });

  fetcher_->ExecuteModel(ModelBasedCapabilityKey::kZeroStateSuggestions,
                         /*identity_manager=*/nullptr, request,
                         /*timeout=*/std::nullopt, base::DoNothing());
}
TEST_F(PrivateAiModelExecutionFetcherTest,
       ConvertsZeroStateSuggestionsRequestWithLists) {
  proto::ZeroStateSuggestionsRequest request;
  auto* list = request.mutable_page_context_list();
  auto* context1 = list->add_page_contexts()->mutable_page_context();
  context1->set_url("url1");
  context1->set_title("Привіт");
  auto* context2 = list->add_page_contexts()->mutable_page_context();
  context2->set_url("url2");
  context2->set_title("你好");

  EXPECT_CALL(
      mock_private_ai_client_,
      SendPaicRequest(
          testing::Eq(
              private_ai::proto::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION),
          testing::_, testing::_,
          testing::Field(&private_ai::Client::RequestOptions::timeout,
                         private_ai::Client::kDefaultTimeout)))
      .WillOnce(
          [](private_ai::proto::FeatureName feature_name,
             const private_ai::proto::PaicMessage& request,
             private_ai::Client::OnPaicMessageRequestCompletedCallback callback,
             const private_ai::Client::RequestOptions& options) {
            auto execute_request = request.execute_request_ext();
            auto zss_request =
                ParsedAnyMetadata<proto::ZeroStateSuggestionsRequest>(
                    execute_request.request_metadata());
            const auto& page_context_list = zss_request->page_context_list();
            EXPECT_EQ(page_context_list.page_contexts(0).page_context().url(),
                      "url1");
            EXPECT_EQ(page_context_list.page_contexts(0).page_context().title(),
                      "Привіт");
            EXPECT_EQ(page_context_list.page_contexts(1).page_context().url(),
                      "url2");
            EXPECT_EQ(page_context_list.page_contexts(1).page_context().title(),
                      "你好");
            std::move(callback).Run(base::ok(private_ai::proto::PaicMessage()));
          });

  fetcher_->ExecuteModel(ModelBasedCapabilityKey::kZeroStateSuggestions,
                         /*identity_manager=*/nullptr, request,
                         /*timeout=*/std::nullopt, base::DoNothing());
}

TEST_F(PrivateAiModelExecutionFetcherTest,
       ConvertsZeroStateSuggestionsResponse) {
  EXPECT_CALL(
      mock_private_ai_client_,
      SendPaicRequest(
          testing::Eq(
              private_ai::proto::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION),
          testing::_, testing::_,
          testing::Field(&private_ai::Client::RequestOptions::timeout,
                         private_ai::Client::kDefaultTimeout)))
      .WillOnce(
          [](private_ai::proto::FeatureName feature_name,
             const private_ai::proto::PaicMessage& request,
             private_ai::Client::OnPaicMessageRequestCompletedCallback callback,
             const private_ai::Client::RequestOptions& options) {
            private_ai::proto::PaicMessage response;
            proto::ZeroStateSuggestionsResponse zss_response;
            zss_response.add_suggestions()->set_label("Hello");
            zss_response.add_suggestions()->set_label("Привіт");
            zss_response.add_suggestions()->set_label("你好");
            *response.mutable_execute_response_ext()
                 ->mutable_response_metadata() = AnyWrapProto(zss_response);
            std::move(callback).Run(base::ok(response));
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

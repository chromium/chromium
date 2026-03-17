// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/core/finds_service.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/optimization_guide/proto/features/finds.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace history {
class MockHistoryService : public HistoryService {
 public:
  MockHistoryService() = default;
  ~MockHistoryService() override = default;

  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              QueryHistory,
              (const std::u16string& text_query,
               const QueryOptions& options,
               QueryHistoryCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
};
}  // namespace history

namespace finds {

class FindsServiceTest : public testing::Test {
 public:
  FindsServiceTest() = default;
  ~FindsServiceTest() override = default;

  void SetUp() override {
    opt_guide_service_ = std::make_unique<
        testing::NiceMock<MockOptimizationGuideKeyedService>>();
    history_service_ =
        std::make_unique<testing::NiceMock<history::MockHistoryService>>();
    service_ = std::make_unique<FindsService>(opt_guide_service_.get(),
                                              history_service_.get());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<MockOptimizationGuideKeyedService> opt_guide_service_;
  std::unique_ptr<history::MockHistoryService> history_service_;
  std::unique_ptr<FindsService> service_;
};

TEST_F(FindsServiceTest, HistoryServiceUnavailable) {
  auto service =
      std::make_unique<FindsService>(opt_guide_service_.get(), nullptr);

  bool callback_called = false;
  service->GetModelResponse(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kHistoryServiceUnavailable,
                  result.status);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
}

TEST_F(FindsServiceTest, OptimizationGuideUnavailable) {
  auto service =
      std::make_unique<FindsService>(nullptr, history_service_.get());

  bool callback_called = false;
  service->GetModelResponse(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kOptimizationGuideUnavailable,
                  result.status);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
}

TEST_F(FindsServiceTest, EmptyHistory) {
  EXPECT_CALL(*history_service_, QueryHistory(_, _, _, _))
      .WillOnce([](const std::u16string& text_query,
                   const history::QueryOptions& options,
                   history::HistoryService::QueryHistoryCallback callback,
                   base::CancelableTaskTracker* tracker) {
        std::move(callback).Run(history::QueryResults());
        return base::CancelableTaskTracker::kBadTaskId;
      });

  bool callback_called = false;
  service_->GetModelResponse(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kEmptyHistory, result.status);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
}

TEST_F(FindsServiceTest, ModelExecutionFailed) {
  EXPECT_CALL(*history_service_, QueryHistory(_, _, _, _))
      .WillOnce([](const std::u16string& text_query,
                   const history::QueryOptions& options,
                   history::HistoryService::QueryHistoryCallback callback,
                   base::CancelableTaskTracker* tracker) {
        history::QueryResults results;
        std::vector<history::URLResult> urls;
        urls.emplace_back(GURL("https://example.com"), base::Time::Now());
        results.SetURLResults(std::move(urls));
        std::move(callback).Run(std::move(results));
        return base::CancelableTaskTracker::kBadTaskId;
      });

  EXPECT_CALL(*opt_guide_service_, ExecuteModel(_, _, _, _))
      .WillOnce([](optimization_guide::ModelBasedCapabilityKey feature,
                   const google::protobuf::MessageLite& request_metadata,
                   const optimization_guide::ModelExecutionOptions&
                       execution_options,
                   optimization_guide::
                       OptimizationGuideModelExecutionResultCallback callback) {
        optimization_guide::OptimizationGuideModelExecutionResult result;
        result.response = base::unexpected(
            optimization_guide::OptimizationGuideModelExecutionError::
                FromModelExecutionError(
                    optimization_guide::OptimizationGuideModelExecutionError::
                        ModelExecutionError::kGenericFailure));
        std::move(callback).Run(std::move(result), nullptr);
      });

  bool callback_called = false;
  service_->GetModelResponse(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kModelExecutionFailed,
                  result.status);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
}

TEST_F(FindsServiceTest, ParsingFailed) {
  EXPECT_CALL(*history_service_, QueryHistory(_, _, _, _))
      .WillOnce([](const std::u16string& text_query,
                   const history::QueryOptions& options,
                   history::HistoryService::QueryHistoryCallback callback,
                   base::CancelableTaskTracker* tracker) {
        history::QueryResults results;
        std::vector<history::URLResult> urls;
        urls.emplace_back(GURL("https://example.com"), base::Time::Now());
        results.SetURLResults(std::move(urls));
        std::move(callback).Run(std::move(results));
        return base::CancelableTaskTracker::kBadTaskId;
      });

  EXPECT_CALL(*opt_guide_service_, ExecuteModel(_, _, _, _))
      .WillOnce(
          [](optimization_guide::ModelBasedCapabilityKey feature,
             const google::protobuf::MessageLite& request_metadata,
             const optimization_guide::ModelExecutionOptions& execution_options,
             optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            optimization_guide::OptimizationGuideModelExecutionResult result;
            optimization_guide::proto::Any any;
            any.set_type_url("type.googleapis.com/not.a.match");
            result.response = any;
            std::move(callback).Run(std::move(result), nullptr);
          });

  bool callback_called = false;
  service_->GetModelResponse(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kResponseParsingFailed,
                  result.status);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
}

TEST_F(FindsServiceTest, Success) {
  EXPECT_CALL(*history_service_, QueryHistory(_, _, _, _))
      .WillOnce([](const std::u16string& text_query,
                   const history::QueryOptions& options,
                   history::HistoryService::QueryHistoryCallback callback,
                   base::CancelableTaskTracker* tracker) {
        history::QueryResults results;
        std::vector<history::URLResult> urls;
        urls.emplace_back(GURL("https://example.com"), base::Time::Now());
        results.SetURLResults(std::move(urls));
        std::move(callback).Run(std::move(results));
        return base::CancelableTaskTracker::kBadTaskId;
      });

  EXPECT_CALL(*opt_guide_service_, ExecuteModel(_, _, _, _))
      .WillOnce(
          [](optimization_guide::ModelBasedCapabilityKey feature,
             const google::protobuf::MessageLite& request_metadata,
             const optimization_guide::ModelExecutionOptions& execution_options,
             optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            optimization_guide::OptimizationGuideModelExecutionResult result;
            optimization_guide::proto::FindsSuggestionResponse response;
            auto* suggestion = response.add_suggestions();
            suggestion->set_theme_title("Shopping");
            optimization_guide::proto::Any any;
            any.set_type_url(
                "type.googleapis.com/"
                "optimization_guide.proto.FindsSuggestionResponse");
            response.SerializeToString(any.mutable_value());
            result.response = any;
            std::move(callback).Run(std::move(result), nullptr);
          });

  bool callback_called = false;
  service_->GetModelResponse(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kSuccess, result.status);
        EXPECT_EQ("Theme: Shopping (Type: 0, Score: 0)", result.message);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
}

}  // namespace finds

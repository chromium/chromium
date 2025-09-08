// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/login_state_checker.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/password_change/annotated_page_content_capturer.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::base::test::RunOnceCallback;
using testing::InSequence;
using testing::Invoke;
using testing::WithArg;

std::unique_ptr<KeyedService> CreateOptimizationService(
    content::BrowserContext* context) {
  return std::make_unique<MockOptimizationGuideKeyedService>();
}

template <bool state>
void PostResponse(
    optimization_guide::OptimizationGuideModelExecutionResultCallback
        callback) {
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_is_logged_in_data()->set_is_logged_in(state);
  auto result = optimization_guide::OptimizationGuideModelExecutionResult(
      optimization_guide::AnyWrapProto(response),
      /*execution_info=*/nullptr);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result),
                                /*log_entry=*/nullptr));
}

void VerifyRetryCount(const optimization_guide::proto::LogAiDataRequest& log,
                      const int expected_retry_count) {
  EXPECT_EQ(log.password_change_submission()
                .quality()
                .logged_in_check()
                .retry_count(),
            expected_retry_count);
}

}  // namespace

class LoginStateCheckerTest : public ChromeRenderViewHostTestHarness {
 public:
  LoginStateCheckerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~LoginStateCheckerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&CreateOptimizationService));
    logs_uploader_ =
        std::make_unique<ModelQualityLogsUploader>(web_contents(), GURL());
  }

  void TearDown() override {
    logs_uploader_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<LoginStateChecker> CreateChecker(
      LoginStateChecker::LoginStateResultCallback callback) {
    return std::make_unique<LoginStateChecker>(
        web_contents(), logs_uploader_.get(), nullptr, std::move(callback));
  }

  const std::unique_ptr<ModelQualityLogsUploader>& logs_uploader() {
    return logs_uploader_;
  }

  MockOptimizationGuideKeyedService* optimization_service() {
    return static_cast<MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile()));
  }

 private:
  std::unique_ptr<ModelQualityLogsUploader> logs_uploader_;
};

TEST_F(LoginStateCheckerTest, UserIsLoggedInOnFirstAttempt) {
  base::test::TestFuture<bool> future;
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(Invoke(&PostResponse<true>)));

  auto checker = CreateChecker(future.GetRepeatingCallback());
  ASSERT_TRUE(checker->capturer());
  checker->capturer()->ReplyWithContent(
      optimization_guide::AIPageContentResult());
  EXPECT_TRUE(future.Take());
  VerifyRetryCount(logs_uploader()->GetFinalLog(), 0);
}

TEST_F(LoginStateCheckerTest, UserIsLoggedInOnSecondAttempt) {
  base::test::TestFuture<bool> future;
  {
    InSequence s;
    EXPECT_CALL(*optimization_service(), ExecuteModel)
        .WillOnce(WithArg<3>(Invoke(&PostResponse<false>)));
    EXPECT_CALL(*optimization_service(), ExecuteModel)
        .WillOnce(WithArg<3>(Invoke(&PostResponse<true>)));
  }

  auto checker = CreateChecker(future.GetRepeatingCallback());
  // First model call should be negative, the user is not logged in.
  checker->capturer()->ReplyWithContent(
      optimization_guide::AIPageContentResult());
  EXPECT_FALSE(future.Take());

  // Simulate finishing a navigation in the main frame.
  static_cast<content::WebContentsObserver*>(checker.get())
      ->DidFinishNavigation(nullptr);
  // Second model call should be positive, the user is logged in.
  checker->capturer()->ReplyWithContent(
      optimization_guide::AIPageContentResult());
  EXPECT_TRUE(future.Take());
  VerifyRetryCount(logs_uploader()->GetFinalLog(), 1);
}

TEST_F(LoginStateCheckerTest, FailsAfterPageContentCaptureFailure) {
  base::test::TestFuture<bool> future;
  auto checker = CreateChecker(future.GetRepeatingCallback());
  ASSERT_TRUE(checker->capturer());
  checker->capturer()->ReplyWithContent(std::nullopt);
  EXPECT_FALSE(future.Take());
  VerifyRetryCount(logs_uploader()->GetFinalLog(), 0);
}

TEST_F(LoginStateCheckerTest, ExceedsMaxLoginChecksAndFails) {
  base::test::TestFuture<bool> future;
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .Times(LoginStateChecker::kMaxLoginChecks)
      .WillRepeatedly(WithArg<3>(Invoke(&PostResponse<false>)));

  auto checker = CreateChecker(future.GetRepeatingCallback());
  for (int i = 0; i < LoginStateChecker::kMaxLoginChecks; ++i) {
    checker->capturer()->ReplyWithContent(
        optimization_guide::AIPageContentResult());
    EXPECT_FALSE(future.Take());

    if (i < LoginStateChecker::kMaxLoginChecks - 1) {
      EXPECT_FALSE(checker->ReachedAttemptsLimit());
      static_cast<content::WebContentsObserver*>(checker.get())
          ->DidFinishNavigation(nullptr);
    }
  }
  // The next check should fail immediately without calling the model.
  EXPECT_CALL(*optimization_service(), ExecuteModel).Times(0);
  EXPECT_TRUE(checker->ReachedAttemptsLimit());
  static_cast<content::WebContentsObserver*>(checker.get())
      ->DidFinishNavigation(nullptr);
  EXPECT_FALSE(future.Take());
  VerifyRetryCount(logs_uploader()->GetFinalLog(),
                   LoginStateChecker::kMaxLoginChecks - 1);
}

TEST_F(LoginStateCheckerTest, CachesPageContentIfRequestInFlight) {
  base::test::TestFuture<bool> future;

  auto checker = CreateChecker(future.GetRepeatingCallback());
  ASSERT_TRUE(checker->capturer());

  // Trigger first request.
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      optimization_guide_callback;
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(MoveArg<3>(&optimization_guide_callback));
  checker->capturer()->ReplyWithContent(
      optimization_guide::AIPageContentResult());
  ASSERT_TRUE(optimization_guide_callback);

  // Trigger second request while first is in flight. This should be cached.
  testing::Mock::VerifyAndClearExpectations(optimization_service());
  EXPECT_CALL(*optimization_service(), ExecuteModel).Times(0);
  static_cast<content::WebContentsObserver*>(checker.get())
      ->DidFinishNavigation(nullptr);
  checker->capturer()->ReplyWithContent(
      optimization_guide::AIPageContentResult());

  // First request finishes with a failure.
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(MoveArg<3>(&optimization_guide_callback));
  PostResponse<false>(std::move(optimization_guide_callback));
  EXPECT_FALSE(future.Take());
  ASSERT_TRUE(optimization_guide_callback);

  // Second request should be processed now and succeed.
  PostResponse<true>(std::move(optimization_guide_callback));
  EXPECT_TRUE(future.Take());
}

TEST_F(LoginStateCheckerTest, CachesOnlyLastPageContent) {
  base::test::TestFuture<bool> future;
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      optimization_guide_callback;

  auto checker = CreateChecker(future.GetRepeatingCallback());
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(MoveArg<3>(&optimization_guide_callback));
  checker->capturer()->ReplyWithContent(
      optimization_guide::AIPageContentResult());
  ASSERT_TRUE(optimization_guide_callback);

  // These two replies should come while the first request is in flight.
  // Only the second one should be processed.
  static_cast<content::WebContentsObserver*>(checker.get())
      ->DidFinishNavigation(nullptr);
  checker->capturer()->ReplyWithContent(
      optimization_guide::AIPageContentResult());
  static_cast<content::WebContentsObserver*>(checker.get())
      ->DidFinishNavigation(nullptr);
  checker->capturer()->ReplyWithContent(
      optimization_guide::AIPageContentResult());

  // Only the last cache is used, resulting into a single call to
  // `ExecuteModel`.
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .Times(1)
      .WillOnce(MoveArg<3>(&optimization_guide_callback));
  PostResponse<false>(std::move(optimization_guide_callback));
  EXPECT_FALSE(future.Take());
  ASSERT_TRUE(optimization_guide_callback);

  PostResponse<true>(std::move(optimization_guide_callback));
  EXPECT_TRUE(future.Take());
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/login_state_checker.h"

#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/password_change/annotated_page_content_capturer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

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
  }

  std::unique_ptr<LoginStateChecker> CreateChecker(
      LoginStateChecker::InitialLoginCheckFailedCallback first_check_callback,
      LoginStateChecker::LoginStateResultCallback final_check_callback) {
    return std::make_unique<LoginStateChecker>(web_contents(), nullptr,
                                               std::move(first_check_callback),
                                               std::move(final_check_callback));
  }

  MockOptimizationGuideKeyedService* optimization_service() {
    return static_cast<MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile()));
  }
};

TEST_F(LoginStateCheckerTest, UserIsLoggedInOnFirstAttempt) {
  base::test::TestFuture<void> first_future;
  base::test::TestFuture<bool> last_future;
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(Invoke(&PostResponse<true>)));

  auto checker =
      CreateChecker(first_future.GetCallback(), last_future.GetCallback());
  ASSERT_TRUE(checker->capturer());
  checker->capturer()->ReplyWithContent(
      optimization_guide::AIPageContentResult());
  EXPECT_TRUE(last_future.Get());
  EXPECT_FALSE(first_future.IsReady());
}

TEST_F(LoginStateCheckerTest, UserIsLoggedInOnSecondAttempt) {
  base::test::TestFuture<void> first_future;
  base::test::TestFuture<bool> last_future;
  {
    InSequence s;
    EXPECT_CALL(*optimization_service(), ExecuteModel)
        .WillOnce(WithArg<3>(Invoke(&PostResponse<false>)));
    EXPECT_CALL(*optimization_service(), ExecuteModel)
        .WillOnce(WithArg<3>(Invoke(&PostResponse<true>)));
  }

  auto checker =
      CreateChecker(first_future.GetCallback(), last_future.GetCallback());
  // First model call should be negative, the user is not logged in.
  checker->capturer()->ReplyWithContent(
      optimization_guide::AIPageContentResult());
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(first_future.IsReady());
  EXPECT_FALSE(last_future.IsReady());

  // Simulate finishing a navigation in the main frame.
  static_cast<content::WebContentsObserver*>(checker.get())
      ->DidFinishNavigation(nullptr);
  // Second model call should be positive, the user is logged in.
  checker->capturer()->ReplyWithContent(
      optimization_guide::AIPageContentResult());
  EXPECT_TRUE(last_future.Get());
}

TEST_F(LoginStateCheckerTest, FailsAfterPageContentCaptureFailure) {
  base::test::TestFuture<void> first_future;
  base::test::TestFuture<bool> last_future;
  auto checker =
      CreateChecker(first_future.GetCallback(), last_future.GetCallback());
  ASSERT_TRUE(checker->capturer());
  checker->capturer()->ReplyWithContent(std::nullopt);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(first_future.IsReady());
  EXPECT_FALSE(last_future.Get());
}

TEST_F(LoginStateCheckerTest, ExceedsMaxLoginChecksAndFails) {
  base::test::TestFuture<void> first_future;
  base::test::TestFuture<bool> last_future;
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .Times(LoginStateChecker::kMaxLoginChecks)
      .WillRepeatedly(WithArg<3>(Invoke(&PostResponse<false>)));

  auto checker =
      CreateChecker(first_future.GetCallback(), last_future.GetCallback());
  for (int i = 0; i < LoginStateChecker::kMaxLoginChecks - 1; ++i) {
    checker->capturer()->ReplyWithContent(
        optimization_guide::AIPageContentResult());
    static_cast<content::WebContentsObserver*>(checker.get())
        ->DidFinishNavigation(nullptr);
    EXPECT_FALSE(last_future.IsReady());
  }
  // This last call should return false because it exceeded the maximum number
  // of login checks.
  checker->capturer()->ReplyWithContent(
      optimization_guide::AIPageContentResult());
  static_cast<content::WebContentsObserver*>(checker.get())
      ->DidFinishNavigation(nullptr);
  EXPECT_FALSE(last_future.Get());
  EXPECT_FALSE(first_future.IsReady());
}

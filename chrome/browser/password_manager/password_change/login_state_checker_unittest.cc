// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/login_state_checker.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/password_change/annotated_page_content_capturer.h"
#include "chrome/browser/password_manager/password_change/fake_annotated_page_content_capturer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::base::test::RunOnceCallback;
using testing::_;
using testing::Field;
using testing::InSequence;
using testing::WithArg;

MATCHER_P(HasLoginCheckStatus, status, "") {
  return arg.status == status;
}

std::unique_ptr<KeyedService> CreateOptimizationService(
    content::BrowserContext* context) {
  return std::make_unique<MockOptimizationGuideKeyedService>();
}

enum class ResponseType {
  kSuccess,     // Expected response: is_logged_in = true
  kFailure,     // Expected response: is_logged_in = false
  kError,       // Expected response: error_case is set
  kUnexpected,  // Unexpected response.
};

template <ResponseType type>
void PostResponse(
    optimization_guide::OptimizationGuideModelExecutionResultCallback
        callback) {
  std::optional<optimization_guide::proto::Any> server_response;
  switch (type) {
    case ResponseType::kUnexpected: {
      // The expected response is of type `PasswordChangeResponse`, any other
      // proto is unexpected (e.g `PasswordChangeRequest`).
      optimization_guide::proto::PasswordChangeRequest unexpected_response;
      server_response = optimization_guide::AnyWrapProto(unexpected_response);
      break;
    }
    case ResponseType::kSuccess:
    case ResponseType::kFailure: {
      optimization_guide::proto::PasswordChangeResponse response;
      bool is_logged_in = (type == ResponseType::kSuccess);
      response.mutable_is_logged_in_data()->set_is_logged_in(is_logged_in);
      server_response = optimization_guide::AnyWrapProto(response);
      break;
    }
    case ResponseType::kError: {
      optimization_guide::proto::PasswordChangeResponse response;
      response.mutable_is_logged_in_data()->set_error_case(
          optimization_guide::proto::IsLoggedInResponseData::ErrorCase::
              IsLoggedInResponseData_ErrorCase_LOGIN_FAILED);
      response.mutable_is_logged_in_data()->set_is_logged_in(false);
      server_response = optimization_guide::AnyWrapProto(response);
      break;
    }
  }

  auto result = optimization_guide::OptimizationGuideModelExecutionResult(
      std::move(server_response).value(),
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
    AnnotatedPageContentCapturer::SetFactoryForTesting(base::BindRepeating(
        [](content::WebContents* web_contents,
           blink::mojom::AIPageContentOptionsPtr options,
           optimization_guide::OnAIPageContentDone callback)
            -> std::unique_ptr<AnnotatedPageContentCapturer> {
          return std::make_unique<FakeAnnotatedPageContentCapturer>(
              std::move(callback));
        }));
  }

  std::unique_ptr<LoginStateChecker> CreateChecker(
      LoginStateChecker::LoginStateResultCallback callback,
      optimization_guide::ModelExecutionServiceType service_type =
          optimization_guide::ModelExecutionServiceType::kDefault) {
    return std::make_unique<LoginStateChecker>(
        web_contents(), &stub_client_, service_type, std::move(callback));
  }

  MockOptimizationGuideKeyedService* optimization_service() {
    return static_cast<MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile()));
  }

 private:
  password_manager::StubPasswordManagerClient stub_client_;
};

TEST_F(LoginStateCheckerTest, UserIsLoggedInOnFirstAttempt) {
  base::test::TestFuture<LoginCheckResult> future;
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(&PostResponse<ResponseType::kSuccess>));

  std::unique_ptr<LoginStateChecker> checker =
      CreateChecker(future.GetRepeatingCallback());
  ASSERT_TRUE(checker->capturer());
  static_cast<FakeAnnotatedPageContentCapturer*>(checker->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());
  EXPECT_THAT(future.Take(),
              HasLoginCheckStatus(LoginCheckResult::Status::kLoggedIn));
}

TEST_F(LoginStateCheckerTest, UserIsLoggedInOnSecondAttempt) {
  base::test::TestFuture<LoginCheckResult> future;
  {
    InSequence s;
    EXPECT_CALL(*optimization_service(), ExecuteModel)
        .WillOnce(WithArg<3>(&PostResponse<ResponseType::kFailure>));
    EXPECT_CALL(*optimization_service(), ExecuteModel)
        .WillOnce(WithArg<3>(&PostResponse<ResponseType::kSuccess>));
  }

  std::unique_ptr<LoginStateChecker> checker =
      CreateChecker(future.GetRepeatingCallback());
  // First model call should be negative, the user is not logged in.
  static_cast<FakeAnnotatedPageContentCapturer*>(checker->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());
  EXPECT_THAT(future.Take(),
              HasLoginCheckStatus(LoginCheckResult::Status::kLoggedOut));

  // Simulate finishing a navigation in the main frame.
  static_cast<content::WebContentsObserver*>(checker.get())
      ->DidFinishNavigation(nullptr);
  // Second model call should be positive, the user is logged in.
  static_cast<FakeAnnotatedPageContentCapturer*>(checker->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());
  EXPECT_THAT(future.Take(),
              HasLoginCheckStatus(LoginCheckResult::Status::kLoggedIn));
}

TEST_F(LoginStateCheckerTest, FailsAfterUnexpectedResponse) {
  base::test::TestFuture<LoginCheckResult> future;
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(&PostResponse<ResponseType::kUnexpected>));

  std::unique_ptr<LoginStateChecker> checker =
      CreateChecker(future.GetRepeatingCallback());
  ASSERT_TRUE(checker->capturer());
  static_cast<FakeAnnotatedPageContentCapturer*>(checker->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());
  EXPECT_THAT(future.Take(),
              HasLoginCheckStatus(LoginCheckResult::Status::kError));
}

TEST_F(LoginStateCheckerTest, UnexpectedResponseOnSecondAttempt) {
  base::test::TestFuture<LoginCheckResult> future;
  {
    InSequence s;
    EXPECT_CALL(*optimization_service(), ExecuteModel)
        .WillOnce(WithArg<3>(&PostResponse<ResponseType::kFailure>));
    EXPECT_CALL(*optimization_service(), ExecuteModel)
        .WillOnce(WithArg<3>(&PostResponse<ResponseType::kUnexpected>));
  }

  std::unique_ptr<LoginStateChecker> checker =
      CreateChecker(future.GetRepeatingCallback());
  static_cast<FakeAnnotatedPageContentCapturer*>(checker->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());
  EXPECT_THAT(future.Take(),
              HasLoginCheckStatus(LoginCheckResult::Status::kLoggedOut));
  // Simulate finishing a navigation in the main frame to trigger the next
  // check.
  static_cast<content::WebContentsObserver*>(checker.get())
      ->DidFinishNavigation(nullptr);
  static_cast<FakeAnnotatedPageContentCapturer*>(checker->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());
  EXPECT_THAT(future.Take(),
              HasLoginCheckStatus(LoginCheckResult::Status::kError));
}

TEST_F(LoginStateCheckerTest, ExceedsMaxLoginChecksAndFails) {
  base::test::TestFuture<LoginCheckResult> future;
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .Times(LoginStateChecker::kMaxLoginChecks)
      .WillRepeatedly(WithArg<3>(&PostResponse<ResponseType::kFailure>));

  std::unique_ptr<LoginStateChecker> checker =
      CreateChecker(future.GetRepeatingCallback());
  for (int i = 0; i < LoginStateChecker::kMaxLoginChecks; ++i) {
    ASSERT_TRUE(checker->capturer());
    static_cast<FakeAnnotatedPageContentCapturer*>(checker->capturer())
        ->SimulateResponse(optimization_guide::AIPageContentResult());
    EXPECT_THAT(future.Take(),
                HasLoginCheckStatus(LoginCheckResult::Status::kLoggedOut));

    if (i < LoginStateChecker::kMaxLoginChecks - 1) {
      EXPECT_FALSE(checker->ReachedAttemptsLimit());
      static_cast<content::WebContentsObserver*>(checker.get())
          ->DidFinishNavigation(nullptr);
    }
  }
  // Subsequent navigation doesn't trigger a re-check.
  EXPECT_CALL(*optimization_service(), ExecuteModel).Times(0);
  EXPECT_TRUE(checker->ReachedAttemptsLimit());
  static_cast<content::WebContentsObserver*>(checker.get())
      ->DidFinishNavigation(nullptr);
  EXPECT_FALSE(checker->capturer());
}

TEST_F(LoginStateCheckerTest, CachesPageContentIfRequestInFlight) {
  base::test::TestFuture<LoginCheckResult> future;

  std::unique_ptr<LoginStateChecker> checker =
      CreateChecker(future.GetRepeatingCallback());
  ASSERT_TRUE(checker->capturer());

  // Trigger first request.
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      first_optimization_guide_callback;
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(MoveArg<3>(&first_optimization_guide_callback));
  static_cast<FakeAnnotatedPageContentCapturer*>(checker->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());
  ASSERT_TRUE(first_optimization_guide_callback);

  // Trigger second request while first is in flight. This should be cached.
  testing::Mock::VerifyAndClearExpectations(optimization_service());
  EXPECT_CALL(*optimization_service(), ExecuteModel).Times(0);
  static_cast<content::WebContentsObserver*>(checker.get())
      ->DidFinishNavigation(nullptr);
  static_cast<FakeAnnotatedPageContentCapturer*>(checker->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());

  // First request finishes with a failure.
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      second_optimization_guide_callback;
  base::RunLoop run_loop;
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(
          testing::DoAll(testing::Invoke(&run_loop, &base::RunLoop::Quit),
                         MoveArg<3>(&second_optimization_guide_callback)));
  PostResponse<ResponseType::kFailure>(
      std::move(first_optimization_guide_callback));
  EXPECT_THAT(future.Take(),
              HasLoginCheckStatus(LoginCheckResult::Status::kLoggedOut));
  run_loop.Run();

  ASSERT_TRUE(second_optimization_guide_callback);

  // Second request should be processed now and succeed.
  PostResponse<ResponseType::kSuccess>(
      std::move(second_optimization_guide_callback));
  EXPECT_THAT(future.Take(),
              HasLoginCheckStatus(LoginCheckResult::Status::kLoggedIn));
}

TEST_F(LoginStateCheckerTest, CachesOnlyLastPageContent) {
  base::test::TestFuture<LoginCheckResult> future;
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      initial_optimization_guide_callback;

  std::unique_ptr<LoginStateChecker> checker =
      CreateChecker(future.GetRepeatingCallback());
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(MoveArg<3>(&initial_optimization_guide_callback));
  static_cast<FakeAnnotatedPageContentCapturer*>(checker->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());
  ASSERT_TRUE(initial_optimization_guide_callback);

  // These two replies should come while the first request is in flight.
  // Only the second one should be processed.
  static_cast<content::WebContentsObserver*>(checker.get())
      ->DidFinishNavigation(nullptr);
  static_cast<FakeAnnotatedPageContentCapturer*>(checker->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());
  static_cast<content::WebContentsObserver*>(checker.get())
      ->DidFinishNavigation(nullptr);
  static_cast<FakeAnnotatedPageContentCapturer*>(checker->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());

  // Only the last cache is used, resulting into a single call to
  // `ExecuteModel`.
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      cached_optimization_guide_callback;
  base::RunLoop run_loop;
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .Times(1)
      .WillOnce(
          testing::DoAll(testing::Invoke(&run_loop, &base::RunLoop::Quit),
                         MoveArg<3>(&cached_optimization_guide_callback)));
  PostResponse<ResponseType::kFailure>(
      std::move(initial_optimization_guide_callback));
  EXPECT_THAT(future.Take(),
              HasLoginCheckStatus(LoginCheckResult::Status::kLoggedOut));
  run_loop.Run();

  ASSERT_TRUE(cached_optimization_guide_callback);

  // The cached request is processed and succeeds.
  PostResponse<ResponseType::kSuccess>(
      std::move(cached_optimization_guide_callback));
  EXPECT_THAT(future.Take(),
              HasLoginCheckStatus(LoginCheckResult::Status::kLoggedIn));
}

TEST_F(LoginStateCheckerTest, NoRequestWithEmptyCachedPageContent) {
  base::test::TestFuture<LoginCheckResult> future;
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      optimization_guide_callback_1;
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      optimization_guide_callback_2;

  std::unique_ptr<LoginStateChecker> checker =
      CreateChecker(future.GetRepeatingCallback());
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(MoveArg<3>(&optimization_guide_callback_1));
  static_cast<FakeAnnotatedPageContentCapturer*>(checker->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());
  ASSERT_TRUE(optimization_guide_callback_1);

  // A new content is capture while the first request is in
  // flight. This is cached.
  static_cast<content::WebContentsObserver*>(checker.get())
      ->DidFinishNavigation(nullptr);
  static_cast<FakeAnnotatedPageContentCapturer*>(checker->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());

  // Model replies that the user is not logged in.
  // This triggers the cached request.
  base::RunLoop run_loop;
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(testing::DoAll(testing::Invoke(&run_loop, &base::RunLoop::Quit),
                               MoveArg<3>(&optimization_guide_callback_2)));
  PostResponse<ResponseType::kFailure>(
      std::move(optimization_guide_callback_1));
  EXPECT_THAT(future.Take(),
              HasLoginCheckStatus(LoginCheckResult::Status::kLoggedOut));
  run_loop.Run();
  ASSERT_TRUE(optimization_guide_callback_2);

  // The cached request also fails with user not being logged in.
  PostResponse<ResponseType::kFailure>(
      std::move(optimization_guide_callback_2));
  EXPECT_THAT(future.Take(),
              HasLoginCheckStatus(LoginCheckResult::Status::kLoggedOut));

  // Simulate a new navigation which triggers a new login check.
  testing::Mock::VerifyAndClearExpectations(optimization_service());
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(MoveArg<3>(&optimization_guide_callback_1));
  static_cast<content::WebContentsObserver*>(checker.get())
      ->DidFinishNavigation(nullptr);
  // New content is captured and the login check succeeds with it.
  static_cast<FakeAnnotatedPageContentCapturer*>(checker->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());
  ASSERT_TRUE(optimization_guide_callback_1);
  PostResponse<ResponseType::kSuccess>(
      std::move(optimization_guide_callback_1));
  EXPECT_THAT(future.Take(),
              HasLoginCheckStatus(LoginCheckResult::Status::kLoggedIn));
}

TEST_F(LoginStateCheckerTest, FailsAfterErrorInTheResponse) {
  base::test::TestFuture<LoginCheckResult> future;
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(&PostResponse<ResponseType::kUnexpected>));

  std::unique_ptr<LoginStateChecker> checker =
      CreateChecker(future.GetRepeatingCallback());
  ASSERT_TRUE(checker->capturer());
  static_cast<FakeAnnotatedPageContentCapturer*>(checker->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());
  EXPECT_THAT(future.Take(),
              HasLoginCheckStatus(LoginCheckResult::Status::kError));
}

TEST_F(LoginStateCheckerTest, RetryLoginCheck) {
  base::test::TestFuture<LoginCheckResult> future;
  {
    InSequence s;
    EXPECT_CALL(*optimization_service(), ExecuteModel)
        .WillOnce(WithArg<3>(&PostResponse<ResponseType::kFailure>));
    EXPECT_CALL(*optimization_service(), ExecuteModel)
        .WillOnce(WithArg<3>(&PostResponse<ResponseType::kSuccess>));
  }

  std::unique_ptr<LoginStateChecker> checker =
      CreateChecker(future.GetRepeatingCallback());
  // First model call should be negative, the user is not logged in.
  static_cast<FakeAnnotatedPageContentCapturer*>(checker->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());
  EXPECT_THAT(future.Take(),
              HasLoginCheckStatus(LoginCheckResult::Status::kLoggedOut));

  // Trigger a retry.
  checker->RetryLoginCheck();
  // Second model call should be positive, the user is logged in.
  static_cast<FakeAnnotatedPageContentCapturer*>(checker->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());
  EXPECT_THAT(future.Take(),
              HasLoginCheckStatus(LoginCheckResult::Status::kLoggedIn));
}

TEST_F(LoginStateCheckerTest, EmitsHistogramOnCaptureFailure) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<LoginCheckResult> future;

  std::unique_ptr<LoginStateChecker> checker =
      CreateChecker(future.GetRepeatingCallback());
  ASSERT_TRUE(checker->capturer());

  EXPECT_CALL(*optimization_service(), ExecuteModel).Times(0);

  static_cast<FakeAnnotatedPageContentCapturer*>(checker->capturer())
      ->SimulateResponse(base::unexpected("Capture failed"));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChange.FailedCapturingPageContent",
      password_manager::metrics_util::PasswordChangeFlowStep::kLoginCheckStep,
      1);
}

TEST_F(LoginStateCheckerTest, UsesPrivateAiServiceType) {
  base::test::TestFuture<LoginCheckResult> future;
  EXPECT_CALL(
      *optimization_service(),
      ExecuteModel(
          _, _,
          Field(&optimization_guide::ModelExecutionOptions::service_type,
                optimization_guide::ModelExecutionServiceType::kPrivateAi),
          _))
      .WillOnce(WithArg<3>(&PostResponse<ResponseType::kSuccess>));

  std::unique_ptr<LoginStateChecker> checker =
      CreateChecker(future.GetRepeatingCallback(),
                    optimization_guide::ModelExecutionServiceType::kPrivateAi);

  ASSERT_TRUE(checker->capturer());
  static_cast<FakeAnnotatedPageContentCapturer*>(checker->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());
  EXPECT_THAT(future.Take(),
              HasLoginCheckStatus(LoginCheckResult::Status::kLoggedIn));
}

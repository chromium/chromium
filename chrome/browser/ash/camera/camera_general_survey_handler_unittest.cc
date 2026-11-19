// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/camera/camera_general_survey_handler.h"

#include <memory>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Exactly;
using ::testing::Return;

namespace ash {

namespace {

constexpr AccountId::Literal kTestAccountId =
    AccountId::Literal::FromUserEmailGaiaId("test@example.com",
                                            GaiaId::Literal("12345"));

constexpr cros::mojom::CameraClientType kSupportedCameraTypes[] = {
    cros::mojom::CameraClientType::CHROME,
    cros::mojom::CameraClientType::ANDROID,
    cros::mojom::CameraClientType::PLUGINVM,
    cros::mojom::CameraClientType::ASH_CHROME};

constexpr base::TimeDelta kMinCameraOpenDurationForSurveyTest =
    base::Seconds(100);

// Any duration shorter than `kMinCameraOpenDurationForSurveyTest`.
constexpr base::TimeDelta kShortOpenDuration = base::Seconds(10);

// Any duration longer than `kMinCameraOpenDurationForSurveyTest`.
constexpr base::TimeDelta kLongOpenDuration = base::Seconds(110);

constexpr base::TimeDelta kCameraSurveyTriggerTimerDurationTest =
    base::Seconds(5);

// Any duration shorter than `kCameraSurveyTriggerTimerDurationTest`.
constexpr base::TimeDelta kShortCloseDuration = base::Seconds(1);

// Any duration longer than `kCameraSurveyTriggerTimerDurationTest`.
constexpr base::TimeDelta kLongCloseDuration = base::Seconds(10);

class MockDelegate : public CameraGeneralSurveyHandler::Delegate {
 public:
  MOCK_METHOD(void,
              AddActiveCameraClientObserver,
              (media::CameraActiveClientObserver * observer),
              (override));
  MOCK_METHOD(void,
              RemoveActiveCameraClientObserver,
              (media::CameraActiveClientObserver * observer),
              (override));
  MOCK_METHOD(void, LoadConfig, (), (override));
  MOCK_METHOD(bool,
              ShouldShowSurvey,
              (const AccountId& account_id),
              (const, override));
  MOCK_METHOD(void, ShowSurvey, (const AccountId& account_id), (override));
};

class CameraGeneralSurveyHandlerTest : public testing::Test {
 protected:
  CameraGeneralSurveyHandlerTest() = default;

  void InitializeSurveyHandler(std::unique_ptr<MockDelegate> delegate,
                               bool is_enabled = true) {
    survey_handler_ = std::make_unique<CameraGeneralSurveyHandler>(
        is_enabled, std::move(delegate), kMinCameraOpenDurationForSurveyTest,
        kCameraSurveyTriggerTimerDurationTest);
  }

  // testing::Test:
  void SetUp() override {
    user_session_test_environment_ =
        std::make_unique<test::UserSessionTestEnvironment>(
            TestingBrowserProcess::GetGlobal()->local_state());
    ASSERT_TRUE(user_session_test_environment_->AddRegularUser(kTestAccountId));
  }
  void TearDown() override {
    delegate_ = nullptr;
    survey_handler_.reset();
    user_session_test_environment_.reset();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<test::UserSessionTestEnvironment>
      user_session_test_environment_;
  std::unique_ptr<CameraGeneralSurveyHandler> survey_handler_;
  raw_ptr<MockDelegate> delegate_ = nullptr;
};

class CameraGeneralSurveyHandlerInitializationTest
    : public CameraGeneralSurveyHandlerTest,
      public testing::WithParamInterface<bool> {
 protected:
  // testing::Test:
  void SetUp() override {
    CameraGeneralSurveyHandlerTest::SetUp();
    auto delegate = std::make_unique<MockDelegate>();
    delegate_ = delegate.get();
    const bool is_enabled = GetParam();
    InitializeSurveyHandler(std::move(delegate), is_enabled);
    EXPECT_CALL(*delegate_, AddActiveCameraClientObserver)
        .Times(Exactly(is_enabled ? 1 : 0));
    EXPECT_CALL(*delegate_, LoadConfig).Times(Exactly(is_enabled ? 1 : 0));
    user_session_test_environment_->LogIn(kTestAccountId);
  }

  // testing::Test:
  void TearDown() override {
    EXPECT_CALL(*delegate_, RemoveActiveCameraClientObserver)
        .Times(Exactly(GetParam() ? 1 : 0));
    CameraGeneralSurveyHandlerTest::TearDown();
  }
};

TEST_P(CameraGeneralSurveyHandlerInitializationTest, InitializeNoOp) {
  task_environment_.AdvanceClock(kLongOpenDuration);
}

INSTANTIATE_TEST_SUITE_P(CameraGeneralSurveyFeatureEnablement,
                         CameraGeneralSurveyHandlerInitializationTest,
                         testing::Values(true, false));

class CameraGeneralSurveyHandlerOpenCloseTest
    : public CameraGeneralSurveyHandlerTest,
      public testing::WithParamInterface<cros::mojom::CameraClientType> {
 protected:
  void SetUp() override {
    CameraGeneralSurveyHandlerTest::SetUp();
    auto delegate = std::make_unique<MockDelegate>();
    delegate_ = delegate.get();
    InitializeSurveyHandler(std::move(delegate));
    EXPECT_CALL(*delegate_, AddActiveCameraClientObserver).Times(Exactly(1));
    EXPECT_CALL(*delegate_, LoadConfig).Times(Exactly(1));
    user_session_test_environment_->LogIn(kTestAccountId);
  }
  void TearDown() override {
    EXPECT_CALL(*delegate_, RemoveActiveCameraClientObserver).Times(Exactly(1));
    CameraGeneralSurveyHandlerTest::TearDown();
  }

  // Simulates opening a camera and then keeping it open for `duration`.
  void OpenCamera(base::TimeDelta duration = base::Seconds(0)) {
    survey_handler_->OnActiveClientChange(GetParam(),
                                          /*is_new_active_client*/ true,
                                          base::flat_set<std::string>({"0"}));
    task_environment_.FastForwardBy(duration);
  }

  // Simulates closing all cameras and then keeping them closed for `duration`.
  void CloseCamera(base::TimeDelta duration = base::Seconds(0)) {
    survey_handler_->OnActiveClientChange(GetParam(),
                                          /*is_new_active_client*/ false,
                                          base::flat_set<std::string>({}));
    task_environment_.FastForwardBy(duration);
  }
};

TEST_P(CameraGeneralSurveyHandlerOpenCloseTest, CameraOpenNotYetClosed) {
  // It shouldn't show survey when camera is still open.
  EXPECT_CALL(*delegate_, ShowSurvey).Times(Exactly(0));

  OpenCamera(kLongOpenDuration);
}

TEST_P(CameraGeneralSurveyHandlerOpenCloseTest, CameraOpenShortTime) {
  // It shouldn't show survey after camera was just open for a duration
  // shorter than `kMinCameraOpenDurationForSurveyTest`.
  EXPECT_CALL(*delegate_, ShowSurvey).Times(Exactly(0));

  OpenCamera(kShortOpenDuration);
  CloseCamera();
}

TEST_P(CameraGeneralSurveyHandlerOpenCloseTest, CameraReopenImmediately) {
  // It shouldn't show survey after camera was just closed for a duration
  // shorter than `kCameraSurveyTriggerTimerDurationTest` (after being
  // open for duration longer than `kMinCameraOpenDurationForSurveyTest`).
  // Switching between front and back camera falls into this test scenario.
  EXPECT_CALL(*delegate_, ShowSurvey).Times(Exactly(0));
  EXPECT_CALL(*delegate_, ShouldShowSurvey(AccountId(kTestAccountId)))
      .WillOnce(Return(true));

  OpenCamera(kLongOpenDuration);
  CloseCamera(kShortCloseDuration);
  OpenCamera(kLongOpenDuration);
}

TEST_P(CameraGeneralSurveyHandlerOpenCloseTest, ShowSurvey) {
  // It should show survey exactly once after all conditions fulfilled.
  EXPECT_CALL(*delegate_, ShowSurvey(AccountId(kTestAccountId)))
      .Times(Exactly(1));
  EXPECT_CALL(*delegate_, ShouldShowSurvey(AccountId(kTestAccountId)))
      .WillOnce(Return(true));

  OpenCamera(kLongOpenDuration);
  CloseCamera(kLongCloseDuration);
}

TEST_P(CameraGeneralSurveyHandlerOpenCloseTest, UserNotSelectedForSurvey) {
  // All conditions have been fulfilled except that the user is not selected
  // for the survey, therefore it shouldn't show the survey.
  EXPECT_CALL(*delegate_, ShowSurvey).Times(Exactly(0));
  EXPECT_CALL(*delegate_, ShouldShowSurvey(AccountId(kTestAccountId)))
      .WillOnce(Return(false));

  OpenCamera(kLongOpenDuration);
  CloseCamera(kLongCloseDuration);
}

INSTANTIATE_TEST_SUITE_P(CameraGeneralSurveyAllCameraClientTypes,
                         CameraGeneralSurveyHandlerOpenCloseTest,
                         testing::ValuesIn(kSupportedCameraTypes));

}  // namespace

}  // namespace ash

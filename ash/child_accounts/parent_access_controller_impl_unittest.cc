// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/child_accounts/parent_access_controller_impl.h"

#include <string>

#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/login_button.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/pin_request_view.h"
#include "ash/login/ui/pin_request_widget.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/child_accounts/parent_access_controller.h"
#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/button_test_api.h"

namespace ash {

namespace {

using ::testing::_;

AccountId GetChildAccountId() {
  return AccountId::FromUserEmail("child@gmail.com");
}

class ParentAccessControllerImplTest : public LoginTestBase {
 public:
  ParentAccessControllerImplTest(const ParentAccessControllerImplTest&) =
      delete;
  ParentAccessControllerImplTest& operator=(
      const ParentAccessControllerImplTest&) = delete;

 protected:
  ParentAccessControllerImplTest() : account_id_(GetChildAccountId()) {}
  ~ParentAccessControllerImplTest() override = default;

  // LoginScreenTest:
  void SetUp() override {
    LoginTestBase::SetUp();
    login_client_ = std::make_unique<MockLoginScreenClient>();
  }

  void TearDown() override {
    // If the test did not explicitly dismissed the widget, destroy it now.
    PinRequestWidget* pin_request_widget = PinRequestWidget::Get();
    if (pin_request_widget)
      pin_request_widget->Close(false /* validation success */);
    LoginTestBase::TearDown();
  }

  // Simulates mouse press event on a |button|.
  void SimulateButtonPress(views::Button* button) {
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                         gfx::Point(), ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi(button).NotifyClick(event);
  }

  // Called when ParentAccessView finished processing.
  void OnFinished(bool access_granted) {
    access_granted ? ++successful_validation_ : ++back_action_;
  }

  // Starts parent access validation.
  // Use this overloaded method if session state and supervised action are not
  // relevant.
  void StartParentAccess() {
    GetSessionControllerClient()->SetSessionState(
        session_manager::SessionState::LOCKED);
    StartParentAccess(account_id_, SupervisedAction::kUnlockTimeLimits);
  }

  // Starts parent access validation with supervised |action|.
  // Session state should be configured accordingly to the |action|.
  void StartParentAccess(SupervisedAction action) {
    StartParentAccess(account_id_, action);
  }

  // Starts parent access validation with supervised |action| and |account_id|.
  // Session state should be configured accordingly to the |action|.
  void StartParentAccess(const AccountId& account_id, SupervisedAction action) {
    validation_time_ = base::Time::Now();
    ash::ParentAccessController::Get()->ShowWidget(
        account_id,
        base::BindOnce(&ParentAccessControllerImplTest::OnFinished,
                       base::Unretained(this)),
        action, false, validation_time_);
    view_ =
        PinRequestWidget::TestApi(PinRequestWidget::Get()).pin_request_view();
  }

  // Verifies expectation that UMA |action| was logged.
  void ExpectUMAActionReported(ParentAccessControllerImpl::UMAAction action,
                               int bucket_count,
                               int total_count) {
    histogram_tester_.ExpectBucketCount(
        ParentAccessControllerImpl::kUMAParentAccessCodeAction, action,
        bucket_count);
    histogram_tester_.ExpectTotalCount(
        ParentAccessControllerImpl::kUMAParentAccessCodeAction, total_count);
  }

  // Verifies expectation that UMA validation |result| was logged for the
  // |action| and into the aggregated histogram.
  void ExpectUMAValidationResultReported(
      ParentAccessControllerImpl::UMAValidationResult result,
      SupervisedAction action,
      int bucket_count,
      int total_count) {
    const std::string action_result_histogram =
        ParentAccessControllerImpl::GetUMAParentCodeValidationResultHistorgam(
            action);
    histogram_tester_.ExpectBucketCount(action_result_histogram, result,
                                        bucket_count);
    histogram_tester_.ExpectTotalCount(action_result_histogram, total_count);

    const std::string all_results_histogram =
        ParentAccessControllerImpl::GetUMAParentCodeValidationResultHistorgam(
            std::nullopt);

    histogram_tester_.ExpectBucketCount(all_results_histogram, result,
                                        bucket_count);
    histogram_tester_.ExpectTotalCount(all_results_histogram, total_count);
  }

  // Simulates entering a code. |success| determines whether the code will be
  // accepted.
  void SimulateValidation(ParentCodeValidationResult result) {
    login_client_->set_validate_parent_access_code_result(result);
    EXPECT_CALL(*login_client_, ValidateParentAccessCode_(account_id_, "012345",
                                                          validation_time_))
        .Times(1);

    ui::test::EventGenerator* generator = GetEventGenerator();
    for (int i = 0; i < 6; ++i) {
      generator->PressKey(ui::KeyboardCode(ui::KeyboardCode::VKEY_0 + i),
                          ui::EF_NONE);
      base::RunLoop().RunUntilIdle();
    }
  }

  const AccountId account_id_;
  std::unique_ptr<MockLoginScreenClient> login_client_;

  // Number of times the view was dismissed with back button.
  int back_action_ = 0;

  // Number of times the view was dismissed after successful validation.
  int successful_validation_ = 0;

  // Time that will be used on the code validation.
  base::Time validation_time_;

  base::HistogramTester histogram_tester_;

  raw_ptr<PinRequestView, DanglingUntriaged> view_ =
      nullptr;  // Owned by test widget view hierarchy.
};

// Tests parent access dialog showing/hiding and focus behavior for parent
// access.
TEST_F(ParentAccessControllerImplTest, ParentAccessDialogFocus) {
  EXPECT_FALSE(PinRequestWidget::Get());

  StartParentAccess();
  PinRequestView::TestApi view_test_api = PinRequestView::TestApi(view_);

  ASSERT_TRUE(PinRequestWidget::Get());
  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(
      view_test_api.access_code_view()));

  PinRequestWidget::Get()->Close(false /* validation success */);

  EXPECT_FALSE(PinRequestWidget::Get());
}

// Tests correct UMA reporting for parent access.
TEST_F(ParentAccessControllerImplTest, ParentAccessUMARecording) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  StartParentAccess(SupervisedAction::kUnlockTimeLimits);
  histogram_tester_.ExpectBucketCount(
      ParentAccessControllerImpl::kUMAParentAccessCodeUsage,
      ParentAccessControllerImpl::UMAUsage::kTimeLimits, 1);
  SimulateButtonPress(PinRequestView::TestApi(view_).back_button());
  ExpectUMAActionReported(
      ParentAccessControllerImpl::UMAAction::kCanceledByUser, 1, 1);

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  StartParentAccess(SupervisedAction::kUpdateTimezone);
  histogram_tester_.ExpectBucketCount(
      ParentAccessControllerImpl::kUMAParentAccessCodeUsage,
      ParentAccessControllerImpl::UMAUsage::kTimezoneChange, 1);
  SimulateButtonPress(PinRequestView::TestApi(view_).back_button());
  ExpectUMAActionReported(
      ParentAccessControllerImpl::UMAAction::kCanceledByUser, 2, 2);

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  StartParentAccess(SupervisedAction::kUpdateClock);
  histogram_tester_.ExpectBucketCount(
      ParentAccessControllerImpl::kUMAParentAccessCodeUsage,
      ParentAccessControllerImpl::UMAUsage::kTimeChangeInSession, 1);
  SimulateButtonPress(PinRequestView::TestApi(view_).back_button());
  ExpectUMAActionReported(
      ParentAccessControllerImpl::UMAAction::kCanceledByUser, 3, 3);

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  StartParentAccess(EmptyAccountId(), SupervisedAction::kUpdateClock);
  histogram_tester_.ExpectBucketCount(
      ParentAccessControllerImpl::kUMAParentAccessCodeUsage,
      ParentAccessControllerImpl::UMAUsage::kTimeChangeLoginScreen, 1);
  SimulateButtonPress(PinRequestView::TestApi(view_).back_button());
  ExpectUMAActionReported(
      ParentAccessControllerImpl::UMAAction::kCanceledByUser, 4, 4);

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  StartParentAccess(SupervisedAction::kUpdateClock);
  histogram_tester_.ExpectBucketCount(
      ParentAccessControllerImpl::kUMAParentAccessCodeUsage,
      ParentAccessControllerImpl::UMAUsage::kTimeChangeInSession, 2);
  SimulateButtonPress(PinRequestView::TestApi(view_).back_button());
  ExpectUMAActionReported(
      ParentAccessControllerImpl::UMAAction::kCanceledByUser, 5, 5);

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  StartParentAccess(EmptyAccountId(), SupervisedAction::kReauth);
  histogram_tester_.ExpectBucketCount(
      ParentAccessControllerImpl::kUMAParentAccessCodeUsage,
      ParentAccessControllerImpl::UMAUsage::kReauhLoginScreen, 1);
  SimulateButtonPress(PinRequestView::TestApi(view_).back_button());
  ExpectUMAActionReported(
      ParentAccessControllerImpl::UMAAction::kCanceledByUser, 6, 6);

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  StartParentAccess(EmptyAccountId(), SupervisedAction::kAddUser);
  histogram_tester_.ExpectBucketCount(
      ParentAccessControllerImpl::kUMAParentAccessCodeUsage,
      ParentAccessControllerImpl::UMAUsage::kAddUserLoginScreen, 1);
  SimulateButtonPress(PinRequestView::TestApi(view_).back_button());
  ExpectUMAActionReported(
      ParentAccessControllerImpl::UMAAction::kCanceledByUser, 7, 7);

  histogram_tester_.ExpectTotalCount(
      ParentAccessControllerImpl::kUMAParentAccessCodeUsage, 7);
  EXPECT_EQ(7, back_action_);
}

// Tests successful parent access validation flow.
TEST_F(ParentAccessControllerImplTest, ParentAccessSuccessfulValidation) {
  StartParentAccess();
  SimulateValidation(ParentCodeValidationResult::kValid);

  EXPECT_EQ(1, successful_validation_);
  ExpectUMAActionReported(
      ParentAccessControllerImpl::UMAAction::kValidationSuccess, 1, 1);
  ExpectUMAValidationResultReported(
      ParentAccessControllerImpl::UMAValidationResult::kValid,
      SupervisedAction::kUnlockTimeLimits, 1, 1);
}

// Tests unsuccessful parent access flow, including help button and cancelling
// the request.
TEST_F(ParentAccessControllerImplTest, ParentAccessUnsuccessfulValidation) {
  StartParentAccess();
  SimulateValidation(ParentCodeValidationResult::kInvalid);

  ExpectUMAActionReported(
      ParentAccessControllerImpl::UMAAction::kValidationError, 1, 1);
  ExpectUMAValidationResultReported(
      ParentAccessControllerImpl::UMAValidationResult::kInvalid,
      SupervisedAction::kUnlockTimeLimits, 1, 1);

  EXPECT_CALL(*login_client_, ShowParentAccessHelpApp()).Times(1);
  SimulateButtonPress(PinRequestView::TestApi(view_).help_button());
  ExpectUMAActionReported(ParentAccessControllerImpl::UMAAction::kGetHelp, 1,
                          2);

  SimulateButtonPress(PinRequestView::TestApi(view_).back_button());
  ExpectUMAActionReported(
      ParentAccessControllerImpl::UMAAction::kCanceledByUser, 1, 3);
}

TEST_F(ParentAccessControllerImplTest, ParentAccessNoConfig) {
  StartParentAccess();
  SimulateValidation(ParentCodeValidationResult::kNoConfig);

  ExpectUMAActionReported(
      ParentAccessControllerImpl::UMAAction::kValidationError, 1, 1);
  ExpectUMAValidationResultReported(
      ParentAccessControllerImpl::UMAValidationResult::kNoConfig,
      SupervisedAction::kUnlockTimeLimits, 1, 1);
}

TEST_F(ParentAccessControllerImplTest, ParentAccessInternalError) {
  StartParentAccess();
  SimulateValidation(ParentCodeValidationResult::kInternalError);

  ExpectUMAActionReported(
      ParentAccessControllerImpl::UMAAction::kValidationError, 1, 1);
  ExpectUMAValidationResultReported(
      ParentAccessControllerImpl::UMAValidationResult::kInternalError,
      SupervisedAction::kUnlockTimeLimits, 1, 1);
}

#if DCHECK_IS_ON()
// Tests that on login screen we check parent access code against all accounts.
TEST_F(ParentAccessControllerImplTest, EnforceNoAccountSpecifiedOnLogin) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_DEATH_IF_SUPPORTED(
      StartParentAccess(GetChildAccountId(), SupervisedAction::kReauth), "");

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_DEATH_IF_SUPPORTED(
      StartParentAccess(GetChildAccountId(), SupervisedAction::kAddUser), "");

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_DEATH_IF_SUPPORTED(
      StartParentAccess(GetChildAccountId(), SupervisedAction::kUpdateClock),
      "");
}
#endif

}  // namespace
}  // namespace ash

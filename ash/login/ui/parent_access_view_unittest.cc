// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/parent_access_view.h"

#include <memory>
#include <string>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/login_button.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/login/ui/parent_access_widget.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/work_area_insets.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/session_manager_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Struct containing the correct title and description that are displayed when
// the dialog is instantiated with a given ParentAccessRequestReason.
struct ViewModifiersTestData {
  ParentAccessRequestReason reason;
  // The title string id.
  int title;
  // The description string id.
  int description;
};

const ViewModifiersTestData kViewModifiersTestData[] = {
    {ParentAccessRequestReason::kUnlockTimeLimits,
     IDS_ASH_LOGIN_PARENT_ACCESS_TITLE,
     IDS_ASH_LOGIN_PARENT_ACCESS_DESCRIPTION},
    {ParentAccessRequestReason::kChangeTime,
     IDS_ASH_LOGIN_PARENT_ACCESS_TITLE_CHANGE_TIME,
     IDS_ASH_LOGIN_PARENT_ACCESS_GENERIC_DESCRIPTION},
    {ParentAccessRequestReason::kChangeTimezone,
     IDS_ASH_LOGIN_PARENT_ACCESS_TITLE_CHANGE_TIMEZONE,
     IDS_ASH_LOGIN_PARENT_ACCESS_GENERIC_DESCRIPTION}};

// TODO(crbug.com/996828): Make (at least some of) the tests use
// ParentAccessWidget.
class ParentAccessViewTest : public LoginTestBase {
 protected:
  ParentAccessViewTest()
      : account_id_(AccountId::FromUserEmail("child@gmail.com")) {}
  ~ParentAccessViewTest() override = default;

  // LoginScreenTest:
  void SetUp() override {
    LoginTestBase::SetUp();
    login_client_ = std::make_unique<MockLoginScreenClient>();
  }

  void TearDown() override {
    LoginTestBase::TearDown();

    // If the test did not explicitly dismissed the widget, destroy it now.
    if (ParentAccessWidget::Get())
      ParentAccessWidget::Get()->Destroy();
  }

  // Simulates mouse press event on a |button|.
  void SimulateButtonPress(views::Button* button) {
    ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), 0, 0);
    view_->ButtonPressed(button, event);
  }

  // Simulates mouse press event on pin keyboard |button|.
  void SimulatePinKeyboardPress(views::View* button) {
    ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), 0, 0);
    button->OnEvent(&event);
  }

  // Called when ParentAccessView finished processing.
  void OnFinished(bool access_granted) {
    access_granted ? ++successful_validation_ : ++back_action_;
  }

  void StartView(ParentAccessRequestReason reason =
                     ParentAccessRequestReason::kUnlockTimeLimits) {
    ParentAccessView::Callbacks callbacks;
    callbacks.on_finished = base::BindRepeating(
        &ParentAccessViewTest::OnFinished, base::Unretained(this));

    validation_time_ = base::Time::Now();
    view_ =
        new ParentAccessView(account_id_, callbacks, reason, validation_time_);
    SetWidget(CreateWidgetWithContent(view_));
  }

  // Shows parent access widget with the specified |reason|.
  void ShowWidget(ParentAccessRequestReason reason) {
    ParentAccessWidget::Show(
        account_id_,
        base::BindRepeating(&ParentAccessViewTest::OnFinished,
                            base::Unretained(this)),
        reason);
    ParentAccessWidget* widget = ParentAccessWidget::Get();
    ASSERT_TRUE(widget);
  }

  // Dismisses existing parent access widget with back button click. Should be
  // only called when the widget is shown.
  void DismissWidget() {
    ParentAccessWidget* widget = ParentAccessWidget::Get();
    ASSERT_TRUE(widget);

    ParentAccessView* view =
        ParentAccessWidget::TestApi(widget).parent_access_view();
    ParentAccessView::TestApi test_api(view);
    ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), 0, 0);
    view->ButtonPressed(test_api.back_button(), event);
  }

  // Verifies expectation that UMA |action| was logged.
  void ExpectUMAActionReported(ParentAccessView::UMAAction action,
                               int bucket_count,
                               int total_count) {
    histogram_tester_.ExpectBucketCount(
        ParentAccessView::kUMAParentAccessCodeAction, action, bucket_count);
    histogram_tester_.ExpectTotalCount(
        ParentAccessView::kUMAParentAccessCodeAction, total_count);
  }

  void SimulateFailedValidation() {
    login_client_->set_validate_parent_access_code_result(false);
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

  ParentAccessView* view_ = nullptr;  // Owned by test widget view hierarchy.

 private:
  DISALLOW_COPY_AND_ASSIGN(ParentAccessViewTest);
};

class ParentAccessViewModifiersTest
    : public ParentAccessViewTest,
      public ::testing::WithParamInterface<ViewModifiersTestData> {};

}  // namespace

// Tests that title and description are correctly set.
TEST_P(ParentAccessViewModifiersTest, CheckStrings) {
  const ViewModifiersTestData& test_data = GetParam();
  StartView(test_data.reason);
  ParentAccessView::TestApi test_api(view_);
  EXPECT_EQ(l10n_util::GetStringUTF16(test_data.title),
            test_api.title_label()->GetText());
  EXPECT_EQ(l10n_util::GetStringUTF16(test_data.description),
            test_api.description_label()->GetText());
}

INSTANTIATE_TEST_SUITE_P(,
                         ParentAccessViewModifiersTest,
                         testing::ValuesIn(kViewModifiersTestData));

// Tests that back button works.
TEST_F(ParentAccessViewTest, BackButton) {
  StartView();
  ParentAccessView::TestApi test_api(view_);
  EXPECT_TRUE(test_api.back_button()->GetEnabled());
  EXPECT_EQ(0, back_action_);

  SimulateButtonPress(test_api.back_button());

  EXPECT_EQ(1, back_action_);
  EXPECT_EQ(0, successful_validation_);
  ExpectUMAActionReported(ParentAccessView::UMAAction::kCanceledByUser, 1, 1);
}

// Tests that the code is autosubmitted when input is complete.
TEST_F(ParentAccessViewTest, Autosubmit) {
  StartView();
  ParentAccessView::TestApi test_api(view_);
  EXPECT_FALSE(test_api.submit_button()->GetEnabled());

  login_client_->set_validate_parent_access_code_result(true);
  EXPECT_CALL(*login_client_, ValidateParentAccessCode_(account_id_, "012345",
                                                        validation_time_))
      .Times(1);

  ui::test::EventGenerator* generator = GetEventGenerator();
  for (int i = 0; i < 6; ++i) {
    generator->PressKey(ui::KeyboardCode(ui::KeyboardCode::VKEY_0 + i),
                        ui::EF_NONE);
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_EQ(1, successful_validation_);
  ExpectUMAActionReported(ParentAccessView::UMAAction::kValidationSuccess, 1,
                          1);
}

// Tests that submit button submits code from code input.
TEST_F(ParentAccessViewTest, SubmitButton) {
  StartView();
  ParentAccessView::TestApi test_api(view_);
  EXPECT_FALSE(test_api.submit_button()->GetEnabled());
  SimulateFailedValidation();

  auto* generator = GetEventGenerator();
  // Updating input code (here last digit) should clear error state.
  generator->PressKey(ui::KeyboardCode::VKEY_6, ui::EF_NONE);
  EXPECT_EQ(ParentAccessView::State::kNormal, test_api.state());
  EXPECT_TRUE(test_api.submit_button()->GetEnabled());

  login_client_->set_validate_parent_access_code_result(true);
  EXPECT_CALL(*login_client_, ValidateParentAccessCode_(account_id_, "012346",
                                                        validation_time_))
      .Times(1);

  SimulateButtonPress(test_api.submit_button());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, successful_validation_);
  ExpectUMAActionReported(ParentAccessView::UMAAction::kValidationSuccess, 1,
                          2);
}

// Tests that help button opens help app.
TEST_F(ParentAccessViewTest, HelpButton) {
  auto client = std::make_unique<MockLoginScreenClient>();
  StartView();

  ParentAccessView::TestApi test_api(view_);
  EXPECT_TRUE(test_api.help_button()->GetEnabled());

  EXPECT_CALL(*client, ShowParentAccessHelpApp()).Times(1);
  SimulateButtonPress(test_api.help_button());
  base::RunLoop().RunUntilIdle();

  ExpectUMAActionReported(ParentAccessView::UMAAction::kGetHelp, 1, 1);
}

// Tests that access code can be entered with numpad.
TEST_F(ParentAccessViewTest, Numpad) {
  StartView();
  ParentAccessView::TestApi test_api(view_);

  login_client_->set_validate_parent_access_code_result(true);
  EXPECT_CALL(*login_client_, ValidateParentAccessCode_(account_id_, "012345",
                                                        validation_time_))
      .Times(1);
  ui::test::EventGenerator* generator = GetEventGenerator();
  for (int i = 0; i < 6; ++i) {
    generator->PressKey(ui::KeyboardCode(ui::VKEY_NUMPAD0 + i), ui::EF_NONE);
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_TRUE(test_api.submit_button()->GetEnabled());
  EXPECT_EQ(1, successful_validation_);
  ExpectUMAActionReported(ParentAccessView::UMAAction::kValidationSuccess, 1,
                          1);
}

// Tests that access code can be submitted with press of 'enter' key.
TEST_F(ParentAccessViewTest, SubmitWithEnter) {
  StartView();
  ParentAccessView::TestApi test_api(view_);
  EXPECT_FALSE(test_api.submit_button()->GetEnabled());

  SimulateFailedValidation();

  // Updating input code (here last digit) should clear error state.
  auto* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_6, ui::EF_NONE);
  EXPECT_EQ(ParentAccessView::State::kNormal, test_api.state());

  login_client_->set_validate_parent_access_code_result(true);
  EXPECT_CALL(*login_client_, ValidateParentAccessCode_(account_id_, "012346",
                                                        validation_time_))
      .Times(1);

  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, successful_validation_);
  ExpectUMAActionReported(ParentAccessView::UMAAction::kValidationSuccess, 1,
                          2);
}

// Tests that 'enter' key does not submit incomplete code.
TEST_F(ParentAccessViewTest, PressEnterOnIncompleteCode) {
  StartView();
  ParentAccessView::TestApi test_api(view_);
  EXPECT_FALSE(test_api.submit_button()->GetEnabled());

  // Enter incomplete code.
  ui::test::EventGenerator* generator = GetEventGenerator();
  for (int i = 0; i < 5; ++i) {
    generator->PressKey(ui::KeyboardCode(ui::KeyboardCode::VKEY_0 + i),
                        ui::EF_NONE);
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_FALSE(test_api.submit_button()->GetEnabled());

  login_client_->set_validate_parent_access_code_result(true);
  EXPECT_CALL(*login_client_, ValidateParentAccessCode_).Times(0);

  // Pressing enter should not submit incomplete code.
  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, successful_validation_);

  login_client_->set_validate_parent_access_code_result(false);
  EXPECT_CALL(*login_client_, ValidateParentAccessCode_(account_id_, "012349",
                                                        validation_time_))
      .Times(1);

  // Fill in last digit of the code.
  generator->PressKey(ui::KeyboardCode(ui::KeyboardCode::VKEY_9), ui::EF_NONE);
  base::RunLoop().RunUntilIdle();

  // Updating input code (here last digit) should clear error state.
  generator->PressKey(ui::KeyboardCode::VKEY_6, ui::EF_NONE);
  EXPECT_EQ(ParentAccessView::State::kNormal, test_api.state());

  login_client_->set_validate_parent_access_code_result(true);
  EXPECT_CALL(*login_client_, ValidateParentAccessCode_(account_id_, "012346",
                                                        validation_time_))
      .Times(1);

  // Now the code should be submitted with enter key.
  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, successful_validation_);
  ExpectUMAActionReported(ParentAccessView::UMAAction::kValidationSuccess, 1,
                          2);
}

// Tests that backspace button works.
TEST_F(ParentAccessViewTest, Backspace) {
  StartView();
  ParentAccessView::TestApi test_api(view_);
  EXPECT_FALSE(test_api.submit_button()->GetEnabled());

  SimulateFailedValidation();
  EXPECT_EQ(ParentAccessView::State::kError, test_api.state());

  ui::test::EventGenerator* generator = GetEventGenerator();

  // Active field has content - backspace clears the content, but does not move
  // focus.
  generator->PressKey(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);
  EXPECT_FALSE(test_api.submit_button()->GetEnabled());

  // Active Field is empty - backspace moves focus to before last field.
  generator->PressKey(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);
  EXPECT_FALSE(test_api.submit_button()->GetEnabled());

  // Change value in before last field.
  generator->PressKey(ui::KeyboardCode::VKEY_2, ui::EF_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(test_api.submit_button()->GetEnabled());

  // Fill in value in last field.
  generator->PressKey(ui::KeyboardCode::VKEY_3, ui::EF_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_api.submit_button()->GetEnabled());

  login_client_->set_validate_parent_access_code_result(true);
  EXPECT_CALL(*login_client_, ValidateParentAccessCode_(account_id_, "012323",
                                                        validation_time_))
      .Times(1);

  SimulateButtonPress(test_api.submit_button());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, successful_validation_);
  ExpectUMAActionReported(ParentAccessView::UMAAction::kValidationSuccess, 1,
                          2);
}

// Tests input with virtual pin keyboard.
TEST_F(ParentAccessViewTest, PinKeyboard) {
  StartView();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  ParentAccessView::TestApi test_api(view_);
  LoginPinView::TestApi test_pin_keyboard(test_api.pin_keyboard_view());
  EXPECT_FALSE(test_api.submit_button()->GetEnabled());

  login_client_->set_validate_parent_access_code_result(true);
  EXPECT_CALL(*login_client_, ValidateParentAccessCode_(account_id_, "012345",
                                                        validation_time_))
      .Times(1);

  for (int i = 0; i < 6; ++i) {
    SimulatePinKeyboardPress(test_pin_keyboard.GetButton(i));
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_TRUE(test_api.submit_button()->GetEnabled());
  EXPECT_EQ(1, successful_validation_);
  ExpectUMAActionReported(ParentAccessView::UMAAction::kValidationSuccess, 1,
                          1);
}

// Tests that pin keyboard visibility changes upon tablet mode changes.
TEST_F(ParentAccessViewTest, PinKeyboardVisibilityChange) {
  StartView();
  ParentAccessView::TestApi test_api(view_);
  LoginPinView::TestApi test_pin_keyboard(test_api.pin_keyboard_view());
  EXPECT_FALSE(test_api.pin_keyboard_view()->GetVisible());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(test_api.pin_keyboard_view()->GetVisible());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_FALSE(test_api.pin_keyboard_view()->GetVisible());
}

// Tests that error state is shown and cleared when neccesary.
TEST_F(ParentAccessViewTest, ErrorState) {
  StartView();
  ParentAccessView::TestApi test_api(view_);
  EXPECT_EQ(ParentAccessView::State::kNormal, test_api.state());

  // Error should be shown after unsuccessful validation.
  SimulateFailedValidation();
  EXPECT_EQ(ParentAccessView::State::kError, test_api.state());

  EXPECT_EQ(0, successful_validation_);
  ExpectUMAActionReported(ParentAccessView::UMAAction::kValidationError, 1, 1);

  // Updating input code (here last digit) should clear error state.
  auto* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_6, ui::EF_NONE);
  EXPECT_EQ(ParentAccessView::State::kNormal, test_api.state());

  login_client_->set_validate_parent_access_code_result(true);
  EXPECT_CALL(*login_client_, ValidateParentAccessCode_(account_id_, "012346",
                                                        validation_time_))
      .Times(1);

  SimulateButtonPress(test_api.submit_button());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, successful_validation_);

  ExpectUMAActionReported(ParentAccessView::UMAAction::kValidationError, 1, 2);
}

// Tests children views traversal with tab key.
TEST_F(ParentAccessViewTest, TabKeyTraversal) {
  StartView();
  ParentAccessView::TestApi test_api(view_);
  EXPECT_TRUE(HasFocusInAnyChildView(test_api.access_code_view()));

  SimulateFailedValidation();

  // Updating input code (here last digit) should clear error state.
  auto* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_6, ui::EF_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ParentAccessView::State::kNormal, test_api.state());
  EXPECT_TRUE(test_api.submit_button()->HasFocus());

  generator->PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);
  EXPECT_TRUE(test_api.back_button()->HasFocus());

  generator->PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);
  EXPECT_TRUE(HasFocusInAnyChildView(test_api.access_code_view()));

  generator->PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);
  EXPECT_TRUE(test_api.help_button()->HasFocus());
}

// Tests children views backwards traversal with tab key.
TEST_F(ParentAccessViewTest, BackwardTabKeyTraversal) {
  StartView();
  ParentAccessView::TestApi test_api(view_);
  EXPECT_TRUE(HasFocusInAnyChildView(test_api.access_code_view()));

  SimulateFailedValidation();
  auto* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_6, ui::EF_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ParentAccessView::State::kNormal, test_api.state());
  EXPECT_TRUE(test_api.submit_button()->HasFocus());

  generator->PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(test_api.help_button()->HasFocus());

  generator->PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(HasFocusInAnyChildView(test_api.access_code_view()));

  generator->PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(test_api.back_button()->HasFocus());

  generator->PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(test_api.submit_button()->HasFocus());

  generator->PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(test_api.help_button()->HasFocus());

  generator->PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(HasFocusInAnyChildView(test_api.access_code_view()));
}

using ParentAccessWidgetTest = ParentAccessViewTest;

// Tests that correct usage metric is reported.
TEST_F(ParentAccessWidgetTest, UMAUsageMetric) {
  ShowWidget(ParentAccessRequestReason::kUnlockTimeLimits);
  DismissWidget();
  histogram_tester_.ExpectBucketCount(
      ParentAccessView::kUMAParentAccessCodeUsage,
      ParentAccessView::UMAUsage::kTimeLimits, 1);

  ShowWidget(ParentAccessRequestReason::kChangeTimezone);
  DismissWidget();
  histogram_tester_.ExpectBucketCount(
      ParentAccessView::kUMAParentAccessCodeUsage,
      ParentAccessView::UMAUsage::kTimezoneChange, 1);

  // The below usage depends on the session state.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  ShowWidget(ParentAccessRequestReason::kChangeTime);
  DismissWidget();
  histogram_tester_.ExpectBucketCount(
      ParentAccessView::kUMAParentAccessCodeUsage,
      ParentAccessView::UMAUsage::kTimeChangeInSession, 1);

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  ShowWidget(ParentAccessRequestReason::kChangeTime);
  DismissWidget();
  histogram_tester_.ExpectBucketCount(
      ParentAccessView::kUMAParentAccessCodeUsage,
      ParentAccessView::UMAUsage::kTimeChangeLoginScreen, 1);

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  ShowWidget(ParentAccessRequestReason::kChangeTime);
  DismissWidget();
  histogram_tester_.ExpectBucketCount(
      ParentAccessView::kUMAParentAccessCodeUsage,
      ParentAccessView::UMAUsage::kTimeChangeInSession, 2);

  histogram_tester_.ExpectTotalCount(
      ParentAccessView::kUMAParentAccessCodeUsage, 5);
}

// Tests that the widget is properly resized when tablet mode changes.
TEST_F(ParentAccessWidgetTest, WidgetResizingInTabletMode) {
  // Set display large enough to fit preferred view sizes.
  UpdateDisplay("1200x800");
  ShowWidget(ParentAccessRequestReason::kUnlockTimeLimits);

  ParentAccessWidget* widget = ParentAccessWidget::Get();
  ASSERT_TRUE(widget);
  ParentAccessView* view =
      ParentAccessWidget::TestApi(widget).parent_access_view();

  constexpr auto kClamshellModeSize = gfx::Size(340, 340);
  constexpr auto kTabletModeSize = gfx::Size(340, 580);

  const auto widget_size = [&view]() -> gfx::Size {
    return view->GetWidget()->GetWindowBoundsInScreen().size();
  };

  const auto widget_center = [&view]() -> gfx::Point {
    return view->GetWidget()->GetWindowBoundsInScreen().CenterPoint();
  };

  const auto user_work_area_center = [&view]() -> gfx::Point {
    return WorkAreaInsets::ForWindow(view->GetWidget()->GetNativeWindow())
        ->user_work_area_bounds()
        .CenterPoint();
  };

  EXPECT_EQ(kClamshellModeSize, view->size());
  EXPECT_EQ(kClamshellModeSize, widget_size());
  EXPECT_EQ(user_work_area_center(), widget_center());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(kTabletModeSize, view->size());
  EXPECT_EQ(kTabletModeSize, widget_size());
  EXPECT_EQ(user_work_area_center(), widget_center());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_EQ(kClamshellModeSize, view->size());
  EXPECT_EQ(kClamshellModeSize, widget_size());
  EXPECT_EQ(user_work_area_center(), widget_center());
  widget->Destroy();
}

TEST_F(ParentAccessViewTest, VirtualKeyboardHidden) {
  // Enable and show virtual keyboard.
  auto* keyboard_controller = Shell::Get()->keyboard_controller();
  ASSERT_NE(keyboard_controller, nullptr);
  keyboard_controller->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kCommandLineEnabled);

  // Show widget.
  ShowWidget(ParentAccessRequestReason::kUnlockTimeLimits);
  auto* view = ParentAccessWidget::TestApi(ParentAccessWidget::Get())
                   .parent_access_view();
  ParentAccessView::TestApi test_api(view);

  views::Textfield* text_field = test_api.GetInputTextField(0);

  ui::GestureEvent event(
      text_field->x(), text_field->y(), 0, base::TimeTicks::Now(),
      ui::GestureEventDetails(ui::EventType::ET_GESTURE_TAP_DOWN));
  text_field->OnGestureEvent(&event);
  base::RunLoop().RunUntilIdle();

  // Even if we have pressed the text input field, virtual keyboard shouldn't
  // show.
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());

  DismissWidget();
}

// Tests that spoken feedback keycombo starts screen reader.
TEST_F(ParentAccessWidgetTest, SpokenFeedbackKeyCombo) {
  ShowWidget(ParentAccessRequestReason::kUnlockTimeLimits);

  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->spoken_feedback_enabled());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode(ui::KeyboardCode::VKEY_Z),
                      ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(controller->spoken_feedback_enabled());
}

}  // namespace ash

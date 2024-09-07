// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/pin_request_view.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/login_button.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/login/ui/pin_request_widget.h"
#include "ash/public/cpp/login_types.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/work_area_insets.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/session_manager_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/widget.h"

namespace ash {

class PinRequestViewTest : public LoginTestBase,
                           public PinRequestView::Delegate {
 public:
  PinRequestViewTest(const PinRequestViewTest&) = delete;
  PinRequestViewTest& operator=(const PinRequestViewTest&) = delete;

 protected:
  PinRequestViewTest() {}
  ~PinRequestViewTest() override = default;

  // LoginScreenTest:
  void SetUp() override {
    LoginTestBase::SetUp();
    login_client_ = std::make_unique<MockLoginScreenClient>();
  }

  void TearDown() override {
    LoginTestBase::TearDown();

    // If the test did not explicitly dismissed the widget, destroy it now.
    PinRequestWidget* pin_request_widget = PinRequestWidget::Get();
    if (pin_request_widget) {
      pin_request_widget->Close(false /* validation success */);
    }
  }

  PinRequestView::SubmissionResult OnPinSubmitted(
      const std::string& code) override {
    ++pin_submitted_;
    last_code_submitted_ = code;
    if (!will_authenticate_) {
      view_->UpdateState(PinRequestViewState::kError, std::u16string(),
                         std::u16string());
      return PinRequestView::SubmissionResult::kPinError;
    }
    return PinRequestView::SubmissionResult::kPinAccepted;
  }

  void OnBack() override { ++back_action_; }

  void OnHelp() override { ++help_dialog_opened_; }

  void StartView(std::optional<int> pin_length = 6) {
    PinRequest request;
    request.help_button_enabled = true;
    request.obscure_pin = false;
    request.pin_length = pin_length;
    request.title = u"Sample Title";
    request.on_pin_request_done = base::DoNothing();
    view_ = new PinRequestView(std::move(request), this);

    SetWidget(CreateWidgetWithContent(view_));
  }

  // Shows pin request widget with the specified |reason|.
  void ShowWidget(std::optional<int> pin_length = 6) {
    PinRequest request;
    request.help_button_enabled = true;
    request.pin_length = pin_length;
    request.on_pin_request_done = base::DoNothing();
    PinRequestWidget::Show(std::move(request), this);
    PinRequestWidget* widget = PinRequestWidget::Get();
    ASSERT_TRUE(widget);
  }

  // Dismisses existing pin request widget with back button click. Should be
  // only called when the widget is shown.
  void DismissWidget() {
    PinRequestWidget* widget = PinRequestWidget::Get();
    ASSERT_TRUE(widget);

    PinRequestView* view = PinRequestWidget::TestApi(widget).pin_request_view();
    PinRequestView::TestApi test_api(view);
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                         gfx::Point(), ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi(test_api.back_button()).NotifyClick(event);
  }

  void SimulateFailedValidation() {
    will_authenticate_ = false;
    ui::test::EventGenerator* generator = GetEventGenerator();
    for (int i = 0; i < 6; ++i) {
      generator->PressKey(ui::KeyboardCode(ui::KeyboardCode::VKEY_0 + i),
                          ui::EF_NONE);
      base::RunLoop().RunUntilIdle();
    }
    EXPECT_EQ(1, pin_submitted_);
    EXPECT_EQ("012345", last_code_submitted_);
    pin_submitted_ = 0;
  }

  void PressKeyHelper(ui::KeyboardCode key) {
    GetEventGenerator()->PressKey(key, ui::EF_NONE);
    // Wait until the keypress is processed.
    base::RunLoop().RunUntilIdle();
  }

  void ExpectTextSelection(int start, int end) {
    PinRequestView::TestApi test_api(view_);
    ui::AXNodeData ax_node_data;
    test_api.access_code_view()->GetViewAccessibility().GetAccessibleNodeData(
        &ax_node_data);
    EXPECT_EQ(start, ax_node_data.GetIntAttribute(
                         ax::mojom::IntAttribute::kTextSelStart));
    EXPECT_EQ(end, ax_node_data.GetIntAttribute(
                       ax::mojom::IntAttribute::kTextSelEnd));
  }

  void ExpectTextValue(const std::string& value) {
    PinRequestView::TestApi test_api(view_);
    ui::AXNodeData node_data;
    test_api.access_code_view()->GetViewAccessibility().GetAccessibleNodeData(
        &node_data);
    EXPECT_EQ(value,
              node_data.GetStringAttribute(ax::mojom::StringAttribute::kValue));
  }

  std::unique_ptr<MockLoginScreenClient> login_client_;

  // Number of times the Pin was submitted.
  int pin_submitted_ = 0;

  // The code that was submitted the last time.
  std::string last_code_submitted_ = "";

  // Number of times the view was dismissed with back button.
  int back_action_ = 0;

  // Number of times the help dialog was opened.
  int help_dialog_opened_ = 0;

  // Whether the next pin submission will trigger setting an error state.
  bool will_authenticate_ = true;

  raw_ptr<PinRequestView, DanglingUntriaged> view_ =
      nullptr;  // Owned by test widget view hierarchy.
};

// Tests that back button works.
TEST_F(PinRequestViewTest, BackButton) {
  ShowWidget();
  PinRequestWidget* widget = PinRequestWidget::Get();
  view_ = PinRequestWidget::TestApi(widget).pin_request_view();
  PinRequestView::TestApi test_api(view_);
  EXPECT_TRUE(test_api.back_button()->GetEnabled());
  EXPECT_EQ(0, back_action_);

  LeftClickOn(test_api.back_button());

  EXPECT_EQ(1, back_action_);
  EXPECT_EQ(nullptr, PinRequestWidget::Get());
}

// Tests that the code is autosubmitted when input is complete.
TEST_F(PinRequestViewTest, Autosubmit) {
  StartView();
  PinRequestView::TestApi test_api(view_);
  EXPECT_FALSE(test_api.submit_button()->GetEnabled());

  ui::test::EventGenerator* generator = GetEventGenerator();
  for (int i = 0; i < 6; ++i) {
    generator->PressKey(ui::KeyboardCode(ui::KeyboardCode::VKEY_0 + i),
                        ui::EF_NONE);
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_EQ(1, pin_submitted_);
  EXPECT_EQ("012345", last_code_submitted_);
}

// Tests that submit button submits code from code input.
TEST_F(PinRequestViewTest, SubmitButton) {
  StartView();
  PinRequestView::TestApi test_api(view_);
  EXPECT_FALSE(test_api.submit_button()->GetEnabled());
  SimulateFailedValidation();

  // The submit button on the PIN keyboard shouldn't be shown.
  LoginPinView::TestApi test_pin_keyboard(test_api.pin_keyboard_view());
  EXPECT_FALSE(test_pin_keyboard.GetSubmitButton());

  auto* generator = GetEventGenerator();
  // Updating input code (here last digit) should clear error state.
  generator->PressKey(ui::KeyboardCode::VKEY_6, ui::EF_NONE);
  EXPECT_EQ(PinRequestViewState::kNormal, test_api.state());
  EXPECT_TRUE(test_api.submit_button()->GetEnabled());

  LeftClickOn(test_api.submit_button());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, pin_submitted_);
  EXPECT_EQ("012346", last_code_submitted_);
}

// Tests that help button opens help app.
TEST_F(PinRequestViewTest, HelpButton) {
  StartView();

  PinRequestView::TestApi test_api(view_);
  EXPECT_TRUE(test_api.help_button()->GetEnabled());

  LeftClickOn(test_api.help_button());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, help_dialog_opened_);
}

// Tests that access code can be entered with numpad.
TEST_F(PinRequestViewTest, Numpad) {
  StartView();
  PinRequestView::TestApi test_api(view_);

  ui::test::EventGenerator* generator = GetEventGenerator();
  for (int i = 0; i < 6; ++i) {
    generator->PressKey(ui::KeyboardCode(ui::VKEY_NUMPAD0 + i), ui::EF_NONE);
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_TRUE(test_api.submit_button()->GetEnabled());
  EXPECT_EQ(1, pin_submitted_);
  EXPECT_EQ("012345", last_code_submitted_);
}

// Tests that access code can be submitted with press of 'enter' key.
TEST_F(PinRequestViewTest, SubmitWithEnter) {
  StartView();
  PinRequestView::TestApi test_api(view_);
  EXPECT_FALSE(test_api.submit_button()->GetEnabled());

  SimulateFailedValidation();

  // Updating input code (here last digit) should clear error state.
  auto* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_6, ui::EF_NONE);
  EXPECT_EQ(PinRequestViewState::kNormal, test_api.state());

  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, pin_submitted_);
  EXPECT_EQ("012346", last_code_submitted_);
}

// Tests that 'enter' key does not submit incomplete code.
TEST_F(PinRequestViewTest, PressEnterOnIncompleteCode) {
  StartView();
  PinRequestView::TestApi test_api(view_);
  EXPECT_FALSE(test_api.submit_button()->GetEnabled());

  // Enter incomplete code.
  ui::test::EventGenerator* generator = GetEventGenerator();
  for (int i = 0; i < 5; ++i) {
    generator->PressKey(ui::KeyboardCode(ui::KeyboardCode::VKEY_0 + i),
                        ui::EF_NONE);
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_FALSE(test_api.submit_button()->GetEnabled());

  // Pressing enter should not submit incomplete code.
  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, pin_submitted_);

  // Fill in last digit of the code, set error on submission.
  will_authenticate_ = false;
  generator->PressKey(ui::KeyboardCode(ui::KeyboardCode::VKEY_9), ui::EF_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, pin_submitted_);
  EXPECT_EQ("012349", last_code_submitted_);

  // Updating input code (here last digit) should clear error state.
  generator->PressKey(ui::KeyboardCode::VKEY_6, ui::EF_NONE);
  EXPECT_EQ(PinRequestViewState::kNormal, test_api.state());

  // Now the code should be submitted with enter key.
  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, pin_submitted_);
  EXPECT_EQ("012346", last_code_submitted_);
}

// Tests that backspace button works.
TEST_F(PinRequestViewTest, Backspace) {
  StartView();
  PinRequestView::TestApi test_api(view_);
  EXPECT_FALSE(test_api.submit_button()->GetEnabled());

  SimulateFailedValidation();
  EXPECT_EQ(PinRequestViewState::kError, test_api.state());

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

  LeftClickOn(test_api.submit_button());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, pin_submitted_);
  EXPECT_EQ("012323", last_code_submitted_);
}

// Tests digit-only input with unknown pin length.
TEST_F(PinRequestViewTest, FlexCodeInput) {
  StartView(std::nullopt);
  PinRequestView::TestApi test_api(view_);
  ui::test::EventGenerator* generator = GetEventGenerator();
  will_authenticate_ = false;

  EXPECT_FALSE(test_api.submit_button()->GetEnabled());
  for (int i = 0; i < 8; ++i) {
    generator->PressKey(ui::KeyboardCode(ui::KeyboardCode::VKEY_0 + i),
                        ui::EF_NONE);
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_TRUE(test_api.submit_button()->GetEnabled());
  LeftClickOn(test_api.submit_button());
  EXPECT_EQ(1, pin_submitted_);
  EXPECT_EQ("01234567", last_code_submitted_);

  // Test Backspace.
  generator->PressKey(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);
  LeftClickOn(test_api.submit_button());
  EXPECT_EQ(2, pin_submitted_);
  EXPECT_EQ("0123456", last_code_submitted_);
}

// Tests non-digit input with unknown pin length.
TEST_F(PinRequestViewTest, FlexCodeInputCharacters) {
  StartView(std::nullopt);
  PinRequestView::TestApi test_api(view_);
  ui::test::EventGenerator* generator = GetEventGenerator();
  will_authenticate_ = false;

  EXPECT_FALSE(test_api.submit_button()->GetEnabled());

  for (int i = 0; i < 3; ++i) {
    generator->PressKey(ui::KeyboardCode(ui::KeyboardCode::VKEY_A + i),
                        ui::EF_NONE);
    base::RunLoop().RunUntilIdle();
  }
  for (int i = 0; i < 3; ++i) {
    generator->PressKey(ui::KeyboardCode(ui::KeyboardCode::VKEY_A + i),
                        ui::EF_SHIFT_DOWN);
    base::RunLoop().RunUntilIdle();
  }
  generator->PressKey(ui::KeyboardCode(ui::KeyboardCode::VKEY_ADD),
                      ui::EF_NONE);
  generator->PressKey(ui::KeyboardCode(ui::KeyboardCode::VKEY_SUBTRACT),
                      ui::EF_NONE);

  EXPECT_TRUE(test_api.submit_button()->GetEnabled());
  LeftClickOn(test_api.submit_button());
  EXPECT_EQ(1, pin_submitted_);
  EXPECT_EQ("abcABC+-", last_code_submitted_);
}

// Tests input with virtual pin keyboard.
TEST_F(PinRequestViewTest, PinKeyboard) {
  ShowWidget();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  PinRequestWidget* widget = PinRequestWidget::Get();
  view_ = PinRequestWidget::TestApi(widget).pin_request_view();
  PinRequestView::TestApi test_api(view_);
  LoginPinView::TestApi test_pin_keyboard(test_api.pin_keyboard_view());
  EXPECT_FALSE(test_api.submit_button()->GetEnabled());

  for (int i = 0; i < 6; ++i) {
    LeftClickOn(test_pin_keyboard.GetButton(i));
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(1, pin_submitted_);
  EXPECT_EQ("012345", last_code_submitted_);
}

// Tests that pin keyboard visibility changes upon tablet mode changes.
TEST_F(PinRequestViewTest, PinKeyboardVisibilityChange) {
  StartView();
  PinRequestView::TestApi test_api(view_);
  LoginPinView::TestApi test_pin_keyboard(test_api.pin_keyboard_view());
  EXPECT_FALSE(test_api.pin_keyboard_view()->GetVisible());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(test_api.pin_keyboard_view()->GetVisible());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_FALSE(test_api.pin_keyboard_view()->GetVisible());
}

// Tests that error state is shown and cleared when neccesary.
TEST_F(PinRequestViewTest, ErrorState) {
  StartView();
  PinRequestView::TestApi test_api(view_);
  EXPECT_EQ(PinRequestViewState::kNormal, test_api.state());

  // Error should be shown after unsuccessful validation.
  SimulateFailedValidation();
  EXPECT_EQ(PinRequestViewState::kError, test_api.state());

  // Updating input code (here last digit) should clear error state.
  auto* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_6, ui::EF_NONE);
  EXPECT_EQ(PinRequestViewState::kNormal, test_api.state());

  LeftClickOn(test_api.submit_button());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, pin_submitted_);
  EXPECT_EQ("012346", last_code_submitted_);
}

// Tests children views traversal with tab key.
TEST_F(PinRequestViewTest, TabKeyTraversal) {
  StartView();
  PinRequestView::TestApi test_api(view_);
  EXPECT_TRUE(HasFocusInAnyChildView(test_api.access_code_view()));

  SimulateFailedValidation();

  // Updating input code (here last digit) should clear error state.
  auto* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_6, ui::EF_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PinRequestViewState::kNormal, test_api.state());
  EXPECT_TRUE(test_api.submit_button()->HasFocus());

  generator->PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);
  EXPECT_TRUE(test_api.back_button()->HasFocus());

  generator->PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);
  EXPECT_TRUE(HasFocusInAnyChildView(test_api.access_code_view()));

  generator->PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);
  EXPECT_TRUE(test_api.help_button()->HasFocus());
}

// Tests children views backwards traversal with tab key.
TEST_F(PinRequestViewTest, BackwardTabKeyTraversal) {
  StartView();
  PinRequestView::TestApi test_api(view_);
  EXPECT_TRUE(HasFocusInAnyChildView(test_api.access_code_view()));

  SimulateFailedValidation();
  auto* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_6, ui::EF_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PinRequestViewState::kNormal, test_api.state());
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

TEST_F(PinRequestViewTest, AccessibleProperties) {
  StartView();
  ui::AXNodeData data;

  view_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::Role::kDialog, data.role);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Sample Title");
}

class PinRequestWidgetTest : public PinRequestViewTest {
 public:
  PinRequestWidgetTest() { set_start_session(true); }
  PinRequestWidgetTest(const PinRequestWidgetTest&) = delete;
  PinRequestWidgetTest& operator=(const PinRequestWidgetTest&) = delete;
  ~PinRequestWidgetTest() override = default;
};

// Tests that the widget is properly resized when tablet mode changes.
TEST_F(PinRequestWidgetTest, WidgetResizingInTabletMode) {
  // Set display large enough to fit preferred view sizes.
  UpdateDisplay("1200x800");
  ShowWidget();

  PinRequestWidget* widget = PinRequestWidget::Get();
  ASSERT_TRUE(widget);
  PinRequestView* view = PinRequestWidget::TestApi(widget).pin_request_view();

  constexpr auto kClamshellModeSize = gfx::Size(340, 308);
  constexpr auto kTabletModeSize = gfx::Size(340, 532);

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
  widget->Close(false /* validation success */);
}

TEST_F(PinRequestViewTest, VirtualKeyboardHidden) {
  // Enable and show virtual keyboard.
  auto* keyboard_controller = Shell::Get()->keyboard_controller();
  ASSERT_NE(keyboard_controller, nullptr);
  keyboard_controller->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kCommandLineEnabled);

  // Show widget.
  ShowWidget();
  auto* view =
      PinRequestWidget::TestApi(PinRequestWidget::Get()).pin_request_view();
  PinRequestView::TestApi test_api(view);

  views::Textfield* text_field = test_api.GetInputTextField(0);

  ui::GestureEvent event(text_field->x(), text_field->y(), 0,
                         base::TimeTicks::Now(),
                         ui::GestureEventDetails(ui::EventType::kGestureTap));
  text_field->OnGestureEvent(&event);
  base::RunLoop().RunUntilIdle();

  // Even if we have pressed the text input field, virtual keyboard shouldn't
  // show.
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());

  DismissWidget();
}

// Tests input value and text selection of the virtual text field used by a11y.
TEST_F(PinRequestViewTest, VirtualTextFieldForA11y) {
  StartView();
  PinRequestView::TestApi test_api(view_);
  EXPECT_FALSE(test_api.submit_button()->GetEnabled());

  // Assert the initial value.
  PressKeyHelper(ui::KeyboardCode::VKEY_LEFT);
  ExpectTextSelection(0 /*start=*/, 0 /*end=*/);
  ExpectTextValue("      ");

  // Test Insert Digit
  PressKeyHelper(ui::KeyboardCode::VKEY_0);
  ExpectTextSelection(1 /*start=*/, 1 /*end=*/);
  ExpectTextValue("0     ");

  PressKeyHelper(ui::KeyboardCode::VKEY_1);
  ExpectTextSelection(2 /*start=*/, 2 /*end=*/);
  ExpectTextValue("01    ");

  PressKeyHelper(ui::KeyboardCode::VKEY_2);
  ExpectTextSelection(3 /*start=*/, 3 /*end=*/);
  ExpectTextValue("012   ");

  PressKeyHelper(ui::KeyboardCode::VKEY_3);
  ExpectTextSelection(4 /*start=*/, 4 /*end=*/);
  ExpectTextValue("0123  ");

  PressKeyHelper(ui::KeyboardCode::VKEY_4);
  ExpectTextSelection(5 /*start=*/, 5 /*end=*/);
  ExpectTextValue("01234 ");

  PressKeyHelper(ui::KeyboardCode::VKEY_5);
  // It doesn't matter that start != 6 for the last field, since the focus
  // will automatically move to "submit" button, and the last digit won't
  // get read.
  ExpectTextSelection(5 /*start=*/, 6 /*end=*/);
  ExpectTextValue("012345");

  // Test Left Arrow.
  PressKeyHelper(ui::KeyboardCode::VKEY_LEFT);
  ExpectTextSelection(4 /*start=*/, 5 /*end=*/);
  ExpectTextValue("012345");

  PressKeyHelper(ui::KeyboardCode::VKEY_LEFT);
  ExpectTextSelection(3 /*start=*/, 4 /*end=*/);
  ExpectTextValue("012345");

  // Test Right Arrow.
  PressKeyHelper(ui::KeyboardCode::VKEY_RIGHT);
  ExpectTextSelection(4 /*start=*/, 5 /*end=*/);
  ExpectTextValue("012345");

  PressKeyHelper(ui::KeyboardCode::VKEY_RIGHT);
  ExpectTextSelection(5 /*start=*/, 6 /*end=*/);
  ExpectTextValue("012345");

  // Test Backspace.
  PressKeyHelper(ui::KeyboardCode::VKEY_BACK);
  ExpectTextSelection(5 /*start=*/, 5 /*end=*/);
  ExpectTextValue("01234 ");

  PressKeyHelper(ui::KeyboardCode::VKEY_BACK);
  ExpectTextSelection(4 /*start=*/, 4 /*end=*/);
  ExpectTextValue("0123  ");

  // Now the input fields will be [0][1][2][3][|][].

  // Test moving left twice and change value.
  PressKeyHelper(ui::KeyboardCode::VKEY_LEFT);
  PressKeyHelper(ui::KeyboardCode::VKEY_LEFT);
  PressKeyHelper(ui::KeyboardCode::VKEY_3);
  ExpectTextSelection(3 /*start=*/, 4 /*end=*/);
  ExpectTextValue("0133  ");

  // Test Mouse event, mouse click on input field at index 0, then press
  // keyboard 1.
  LeftClickOn(test_api.GetInputTextField(0));
  PressKeyHelper(ui::KeyboardCode::VKEY_1);
  ExpectTextSelection(1 /*start=*/, 2 /*end=*/);
  ExpectTextValue("1133  ");
}

// Tests that spoken feedback keycombo starts screen reader.
TEST_F(PinRequestWidgetTest, SpokenFeedbackKeyCombo) {
  ShowWidget();

  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->spoken_feedback().enabled());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode(ui::KeyboardCode::VKEY_Z),
                      ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(controller->spoken_feedback().enabled());
}

}  // namespace ash

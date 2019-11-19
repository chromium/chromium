// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_password_view.h"

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/login_test_base.h"
#include "base/bind.h"
#include "base/timer/mock_timer.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class LoginPinViewTest : public LoginTestBase {
 protected:
  LoginPinViewTest() = default;
  ~LoginPinViewTest() override = default;

  // Creates login pin view with the specified keyboard |style| and sets it up
  // in a widget.
  void CreateLoginPinViewWithStyle(LoginPinView::Style style) {
    view_ =
        new LoginPinView(style,
                         base::BindRepeating(&LoginPinViewTest::OnPinKey,
                                             base::Unretained(this)),
                         base::BindRepeating(&LoginPinViewTest::OnPinBackspace,
                                             base::Unretained(this)),
                         base::BindRepeating(&LoginPinViewTest::OnPinBack,
                                             base::Unretained(this)));

    SetWidget(CreateWidgetWithContent(view_));
  }

  // Called when a password is submitted.
  void OnPinKey(int value) { value_ = value; }
  void OnPinBackspace() { ++backspace_; }
  void OnPinBack() { ++back_; }

  LoginPinView* view_ = nullptr;  // Owned by test widget view hierarchy.
  base::Optional<int> value_;
  // Number of times the backspace event has been fired.
  int backspace_ = 0;
  // Number of times the back event has been fired.
  int back_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginPinViewTest);
};

}  // namespace

// Verifies that PIN submit works with 'Enter'.
TEST_F(LoginPinViewTest, ButtonsFireEvents) {
  CreateLoginPinViewWithStyle(LoginPinView::Style::kAlphanumeric);
  ui::test::EventGenerator* generator = GetEventGenerator();
  LoginPinView::TestApi test_api(view_);

  // Verify pin button events are emitted with the correct value.
  for (int i = 0; i <= 9; ++i) {
    test_api.GetButton(i)->RequestFocus();
    generator->PressKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);
    EXPECT_TRUE(value_.has_value());
    EXPECT_EQ(*value_, i);
    value_.reset();
  }

  // Verify backspace events are emitted.
  EXPECT_EQ(0, backspace_);
  test_api.GetBackspaceButton()->SetEnabled(true);
  test_api.GetBackspaceButton()->RequestFocus();
  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);
  EXPECT_EQ(1, backspace_);

  EXPECT_EQ(0, back_);
}

// Validates buttons have the correct spacing for alphanumeric PIN keyboard
// style.
TEST_F(LoginPinViewTest, AlphanumericKeyboardButtonSpacingAndSize) {
  CreateLoginPinViewWithStyle(LoginPinView::Style::kAlphanumeric);
  LoginPinView::TestApi test_api(view_);

  const gfx::Size expected_button_size =
      LoginPinView::TestApi::GetButtonSize(LoginPinView::Style::kAlphanumeric);

  // Validate pin button size.
  for (int i = 0; i <= 9; ++i) {
    DCHECK_EQ(test_api.GetButton(i)->size().width(),
              expected_button_size.width());
    DCHECK_EQ(test_api.GetButton(i)->size().height(),
              expected_button_size.height());
  }

  // Validate backspace button size.
  DCHECK_EQ(test_api.GetBackspaceButton()->size().width(),
            expected_button_size.width());
  DCHECK_EQ(test_api.GetBackspaceButton()->size().height(),
            expected_button_size.height());

  // Record all the x/y coordinates of the buttons.
  std::set<int> seen_x;
  std::set<int> seen_y;
  for (int i = 0; i <= 9; ++i) {
    gfx::Rect screen_bounds = test_api.GetButton(i)->GetBoundsInScreen();
    seen_x.insert(screen_bounds.x());
    seen_y.insert(screen_bounds.y());
  }
  seen_x.insert(test_api.GetBackspaceButton()->GetBoundsInScreen().x());
  seen_y.insert(test_api.GetBackspaceButton()->GetBoundsInScreen().y());

  // Sort the coordinates so we can easily check the distance between them.
  std::vector<int> sorted_x(seen_x.begin(), seen_x.end());
  std::vector<int> sorted_y(seen_y.begin(), seen_y.end());
  std::sort(sorted_x.begin(), sorted_x.end());
  std::sort(sorted_y.begin(), sorted_y.end());

  // Validate each x or y coordinate has the correct distance between it and the
  // next one. This is correct because we have already validated button size.
  EXPECT_EQ(3u, sorted_x.size());
  for (size_t i = 0; i < sorted_x.size() - 1; ++i)
    EXPECT_EQ(sorted_x[i] + expected_button_size.width(), sorted_x[i + 1]);

  EXPECT_EQ(4u, sorted_y.size());
  for (size_t i = 0; i < sorted_y.size() - 1; ++i)
    EXPECT_EQ(sorted_y[i] + expected_button_size.height(), sorted_y[i + 1]);
}

// Validates buttons have the correct spacing for numeric PIN keyboard style.
TEST_F(LoginPinViewTest, NumericKeyboardButtonSpacingAndSize) {
  CreateLoginPinViewWithStyle(LoginPinView::Style::kNumeric);
  LoginPinView::TestApi test_api(view_);

  const gfx::Size expected_button_size =
      LoginPinView::TestApi::GetButtonSize(LoginPinView::Style::kNumeric);

  // Validate pin button size.
  for (int i = 0; i <= 9; ++i) {
    DCHECK_EQ(test_api.GetButton(i)->size().width(),
              expected_button_size.width());
    DCHECK_EQ(test_api.GetButton(i)->size().height(),
              expected_button_size.height());
  }

  // Validate backspace button size.
  DCHECK_EQ(test_api.GetBackspaceButton()->size().width(),
            expected_button_size.width());
  DCHECK_EQ(test_api.GetBackspaceButton()->size().height(),
            expected_button_size.height());

  // Record all the x/y coordinates of the buttons.
  std::set<int> seen_x;
  std::set<int> seen_y;
  for (int i = 0; i <= 9; ++i) {
    gfx::Rect screen_bounds = test_api.GetButton(i)->GetBoundsInScreen();
    seen_x.insert(screen_bounds.x());
    seen_y.insert(screen_bounds.y());
  }
  seen_x.insert(test_api.GetBackspaceButton()->GetBoundsInScreen().x());
  seen_y.insert(test_api.GetBackspaceButton()->GetBoundsInScreen().y());

  // Sort the coordinates so we can easily check the distance between them.
  std::vector<int> sorted_x(seen_x.begin(), seen_x.end());
  std::vector<int> sorted_y(seen_y.begin(), seen_y.end());
  std::sort(sorted_x.begin(), sorted_x.end());
  std::sort(sorted_y.begin(), sorted_y.end());

  // Validate each x or y coordinate has the correct distance between it and the
  // next one. This is correct because we have already validated button size.
  EXPECT_EQ(3u, sorted_x.size());
  for (size_t i = 0; i < sorted_x.size() - 1; ++i)
    EXPECT_EQ(sorted_x[i] + expected_button_size.width(), sorted_x[i + 1]);

  EXPECT_EQ(4u, sorted_y.size());
  for (size_t i = 0; i < sorted_y.size() - 1; ++i)
    EXPECT_EQ(sorted_y[i] + expected_button_size.height(), sorted_y[i + 1]);
}

// Verifies that holding the backspace button automatically triggers and begins
// repeating if it is held down.
TEST_F(LoginPinViewTest, BackspaceAutoSubmitsAndRepeats) {
  CreateLoginPinViewWithStyle(LoginPinView::Style::kAlphanumeric);
  ui::test::EventGenerator* generator = GetEventGenerator();
  LoginPinView::TestApi test_api(view_);

  // Install mock timers into the PIN view.
  auto delay_timer0 = std::make_unique<base::MockOneShotTimer>();
  auto repeat_timer0 = std::make_unique<base::MockRepeatingTimer>();
  base::MockOneShotTimer* delay_timer = delay_timer0.get();
  base::MockRepeatingTimer* repeat_timer = repeat_timer0.get();
  test_api.SetBackspaceTimers(std::move(delay_timer0),
                              std::move(repeat_timer0));

  // Verify backspace events are emitted.
  EXPECT_EQ(0, backspace_);
  test_api.GetBackspaceButton()->SetEnabled(true);
  generator->MoveMouseTo(
      test_api.GetBackspaceButton()->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();

  // Backspace event triggers after delay timer fires.
  delay_timer->Fire();
  EXPECT_EQ(1, backspace_);

  // Backspace event triggers after repeat timer fires.
  backspace_ = 0;
  for (int i = 0; i < 5; ++i) {
    repeat_timer->Fire();
    EXPECT_EQ(i + 1, backspace_);
  }

  // Backspace does not trigger after releasing the mouse.
  backspace_ = 0;
  generator->ReleaseLeftButton();
  EXPECT_EQ(0, backspace_);
}

// Verifies that pressing Enter on the "back" button fires the corresponding
// event.
TEST_F(LoginPinViewTest, BackButtonEnter) {
  CreateLoginPinViewWithStyle(LoginPinView::Style::kAlphanumeric);
  ui::test::EventGenerator* generator = GetEventGenerator();
  LoginPinView::TestApi test_api(view_);

  EXPECT_FALSE(test_api.GetBackButton()->GetVisible());

  view_->SetBackButtonVisible(true);
  test_api.GetBackButton()->RequestFocus();
  EXPECT_EQ(0, back_);
  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);
  EXPECT_EQ(1, back_);
}

// Verifies that clicking on the "back" button fires the corresponding event.
TEST_F(LoginPinViewTest, BackButtonClick) {
  CreateLoginPinViewWithStyle(LoginPinView::Style::kAlphanumeric);
  ui::test::EventGenerator* generator = GetEventGenerator();
  LoginPinView::TestApi test_api(view_);

  EXPECT_FALSE(test_api.GetBackButton()->GetVisible());

  view_->SetBackButtonVisible(true);
  generator->MoveMouseTo(
      test_api.GetBackButton()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(0, back_);
  generator->PressLeftButton();
  EXPECT_EQ(1, back_);
}

}  // namespace ash

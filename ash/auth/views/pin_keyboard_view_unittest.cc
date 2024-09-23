// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/pin_keyboard_view.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget.h"

namespace ash {

class PinKeyboardViewUnitTest : public AshTestBase,
                                public PinKeyboardView::Observer,
                                public testing::WithParamInterface<int> {
 public:
  PinKeyboardViewUnitTest() = default;
  PinKeyboardViewUnitTest(const PinKeyboardViewUnitTest&) = delete;
  PinKeyboardViewUnitTest& operator=(const PinKeyboardViewUnitTest&) = delete;
  ~PinKeyboardViewUnitTest() override = default;

  MOCK_METHOD(void, OnDigitButtonPressed, (int), (override));

  MOCK_METHOD(void, OnBackspacePressed, (), (override));

 protected:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);

    PinKeyboardView* view =
        widget_->SetContentsView(std::make_unique<PinKeyboardView>());
    view_test_api_ = std::make_unique<PinKeyboardView::TestApi>(view);
    view_test_api_->AddObserver(this);
  }

  void TearDown() override {
    AshTestBase::TearDown();
    view_test_api_->RemoveObserver(this);
    view_test_api_.reset();
    widget_.reset();
  }

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<PinKeyboardView::TestApi> view_test_api_;
};

// Testing pin keyboard's backspace button click.
// Verifying the observer called properly.
TEST_F(PinKeyboardViewUnitTest, ClickOnBackspace) {
  auto* button_ptr = view_test_api_->backspace_button();
  EXPECT_NE(button_ptr, nullptr);
  EXPECT_TRUE(view_test_api_->GetEnabled());

  EXPECT_CALL(*this, OnBackspacePressed()).Times(1);
  LeftClickOn(button_ptr);
}

// Verifying pin keyboard's SetEnabled triggers the
// property callback.
TEST_F(PinKeyboardViewUnitTest, SetEnabledCallback) {
  bool enabled_changed = false;
  auto subscription =
      view_test_api_->GetView()->AddEnabledChangedCallback(base::BindRepeating(
          [](bool* enabled_changed) { *enabled_changed = true; },
          &enabled_changed));
  EXPECT_TRUE(view_test_api_->GetEnabled());
  view_test_api_->SetEnabled(false);
  EXPECT_TRUE(enabled_changed);
}

// Testing disabled pin keyboard's backspace button click.
// Verifying the observer doesn't called.
TEST_F(PinKeyboardViewUnitTest, ClickOnDisabledBackspace) {
  auto* button_ptr = view_test_api_->backspace_button();
  EXPECT_NE(button_ptr, nullptr);
  EXPECT_TRUE(view_test_api_->GetEnabled());
  view_test_api_->SetEnabled(false);
  EXPECT_FALSE(view_test_api_->GetEnabled());
  EXPECT_CALL(*this, OnBackspacePressed()).Times(0);
  LeftClickOn(button_ptr);
}

// Pin keyboard test with parametrized input.
// Verifying click behavior and observer function across various digits.
TEST_P(PinKeyboardViewUnitTest, ClickOnDigit) {
  int digit = GetParam();
  EXPECT_TRUE(digit >= 0 && digit <= 9);

  auto* button_ptr = view_test_api_->digit_button(digit);
  EXPECT_NE(button_ptr, nullptr);
  EXPECT_TRUE(view_test_api_->GetEnabled());

  EXPECT_CALL(*this, OnDigitButtonPressed(digit)).Times(1);
  LeftClickOn(button_ptr);
}

// Testing disabled pin keyboard with parametrized input.
// Verifying click behavior doesn't call the observer function across various
// digits.
TEST_P(PinKeyboardViewUnitTest, ClickOnDisabledDigit) {
  int digit = GetParam();
  EXPECT_TRUE(digit >= 0 && digit <= 9);

  auto* button_ptr = view_test_api_->digit_button(digit);
  EXPECT_NE(button_ptr, nullptr);
  EXPECT_TRUE(view_test_api_->GetEnabled());
  view_test_api_->SetEnabled(false);
  EXPECT_FALSE(view_test_api_->GetEnabled());
  EXPECT_CALL(*this, OnDigitButtonPressed(digit)).Times(0);
  LeftClickOn(button_ptr);
}

TEST_P(PinKeyboardViewUnitTest, AccessibleProperties) {
  ui::AXNodeData data;

  view_test_api_->GetView()->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.role, ax::mojom::Role::kKeyboard);
}

INSTANTIATE_TEST_SUITE_P(, PinKeyboardViewUnitTest, ::testing::Range(0, 10));

}  // namespace ash

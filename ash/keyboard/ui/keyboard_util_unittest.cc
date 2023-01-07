// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/keyboard/ui/keyboard_ui.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/keyboard/ui/test/test_keyboard_controller_observer.h"
#include "ash/keyboard/ui/test/test_keyboard_layout_delegate.h"
#include "ash/keyboard/ui/test/test_keyboard_ui_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/base/ime/dummy_input_method.h"

namespace keyboard {
namespace {

class KeyboardUtilTest : public aura::test::AuraTestBase {
 public:
  KeyboardUtilTest() = default;

  KeyboardUtilTest(const KeyboardUtilTest&) = delete;
  KeyboardUtilTest& operator=(const KeyboardUtilTest&) = delete;

  ~KeyboardUtilTest() override = default;

  // Sets all flags controlling whether the keyboard should be shown to
  // their disabled state.
  void DisableAllFlags() {
    ResetAllFlags();
    keyboard::SetAccessibilityKeyboardEnabled(false);
    keyboard::SetTouchKeyboardEnabled(false);
    SetEnableFlag(KeyboardEnableFlag::kPolicyDisabled);
    SetEnableFlag(KeyboardEnableFlag::kExtensionDisabled);
  }

  // Sets all flags controlling whether the keyboard should be shown to
  // their enabled flag.
  void EnableAllFlags() {
    ResetAllFlags();
    keyboard::SetAccessibilityKeyboardEnabled(true);
    keyboard::SetTouchKeyboardEnabled(true);
    SetEnableFlag(KeyboardEnableFlag::kPolicyEnabled);
    SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
  }

  // Sets all flags controlling whether the keyboard should be shown to
  // their neutral flag.
  void ResetAllFlags() {
    keyboard::SetAccessibilityKeyboardEnabled(false);
    keyboard::SetTouchKeyboardEnabled(false);
    ClearEnableFlag(KeyboardEnableFlag::kPolicyDisabled);
    ClearEnableFlag(KeyboardEnableFlag::kExtensionDisabled);
    ClearEnableFlag(KeyboardEnableFlag::kPolicyEnabled);
    ClearEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
  }

  void SetUp() override {
    aura::test::AuraTestBase::SetUp();

    layout_delegate_ =
        std::make_unique<TestKeyboardLayoutDelegate>(root_window());
    keyboard_ui_controller_.Initialize(
        std::make_unique<TestKeyboardUIFactory>(&input_method_),
        layout_delegate_.get());

    ResetAllFlags();
  }

  void TearDown() override {
    ResetAllFlags();

    aura::test::AuraTestBase::TearDown();
  }

 protected:
  void SetEnableFlag(KeyboardEnableFlag flag) {
    keyboard_ui_controller_.SetEnableFlag(flag);
  }

  void ClearEnableFlag(KeyboardEnableFlag flag) {
    keyboard_ui_controller_.ClearEnableFlag(flag);
  }

  // Used indirectly by keyboard utils.
  KeyboardUIController keyboard_ui_controller_;
  ui::DummyInputMethod input_method_;
  std::unique_ptr<TestKeyboardLayoutDelegate> layout_delegate_;
};

}  // namespace

// Tests that we respect the accessibility setting.
TEST_F(KeyboardUtilTest, AlwaysShowIfA11yEnabled) {
  // Disabled by default.
  EXPECT_FALSE(IsKeyboardEnabled());
  // If enabled by accessibility, should ignore other flag values.
  DisableAllFlags();
  SetAccessibilityKeyboardEnabled(true);
  EXPECT_TRUE(IsKeyboardEnabled());
}

// Tests that we respect the policy setting.
TEST_F(KeyboardUtilTest, AlwaysShowIfPolicyEnabled) {
  EXPECT_FALSE(IsKeyboardEnabled());
  // If policy is enabled, should ignore other flag values.
  DisableAllFlags();
  SetEnableFlag(KeyboardEnableFlag::kPolicyEnabled);
  EXPECT_TRUE(IsKeyboardEnabled());
}

// Tests that we respect the policy setting.
TEST_F(KeyboardUtilTest, HidesIfPolicyDisabled) {
  EXPECT_FALSE(IsKeyboardEnabled());
  EnableAllFlags();
  // Set accessibility to neutral since accessibility has higher precedence.
  SetAccessibilityKeyboardEnabled(false);
  EXPECT_TRUE(IsKeyboardEnabled());
  // Disable policy. Keyboard should be disabled.
  SetEnableFlag(KeyboardEnableFlag::kPolicyDisabled);
  EXPECT_FALSE(IsKeyboardEnabled());
}

// Tests that the keyboard shows when requested flag provided higher priority
// flags have not been set.
TEST_F(KeyboardUtilTest, ShowKeyboardWhenRequested) {
  DisableAllFlags();
  // Remove device policy, which has higher precedence than us.
  ClearEnableFlag(KeyboardEnableFlag::kPolicyDisabled);
  EXPECT_FALSE(IsKeyboardEnabled());
  // Requested should have higher precedence than all the remaining flags.
  SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
  EXPECT_TRUE(IsKeyboardEnabled());
}

// Tests that the touch keyboard is hidden when requested flag is disabled and
// higher priority flags have not been set.
TEST_F(KeyboardUtilTest, HideKeyboardWhenRequested) {
  EnableAllFlags();
  // Remove higher precedence flags.
  ClearEnableFlag(KeyboardEnableFlag::kPolicyEnabled);
  SetAccessibilityKeyboardEnabled(false);
  EXPECT_TRUE(IsKeyboardEnabled());
  // Set requested flag to disable. Keyboard should disable.
  SetEnableFlag(KeyboardEnableFlag::kExtensionDisabled);
  EXPECT_FALSE(IsKeyboardEnabled());
}

// SetTouchKeyboardEnabled has the lowest priority, but should still work when
// none of the other flags are enabled.
TEST_F(KeyboardUtilTest, HideKeyboardWhenTouchEnabled) {
  ResetAllFlags();
  EXPECT_FALSE(IsKeyboardEnabled());
  SetTouchKeyboardEnabled(true);
  EXPECT_TRUE(IsKeyboardEnabled());
}

TEST_F(KeyboardUtilTest, UpdateKeyboardConfig) {
  ResetAllFlags();
  KeyboardConfig config = keyboard_ui_controller_.keyboard_config();
  EXPECT_TRUE(config.spell_check);
  EXPECT_FALSE(keyboard_ui_controller_.UpdateKeyboardConfig(config));

  config.spell_check = false;
  EXPECT_TRUE(keyboard_ui_controller_.UpdateKeyboardConfig(config));
  EXPECT_FALSE(keyboard_ui_controller_.keyboard_config().spell_check);

  EXPECT_FALSE(keyboard_ui_controller_.UpdateKeyboardConfig(config));
}

TEST_F(KeyboardUtilTest, IsOverscrollEnabled) {
  ResetAllFlags();

  // Return false when keyboard is disabled.
  EXPECT_FALSE(keyboard_ui_controller_.IsKeyboardOverscrollEnabled());

  // Enable the virtual keyboard.
  SetTouchKeyboardEnabled(true);
  EXPECT_TRUE(keyboard_ui_controller_.IsKeyboardOverscrollEnabled());

  // Set overscroll enabled flag.
  KeyboardConfig config = keyboard_ui_controller_.keyboard_config();
  config.overscroll_behavior = KeyboardOverscrollBehavior::kDisabled;
  keyboard_ui_controller_.UpdateKeyboardConfig(config);
  EXPECT_FALSE(keyboard_ui_controller_.IsKeyboardOverscrollEnabled());

  // Set default overscroll flag.
  config.overscroll_behavior = KeyboardOverscrollBehavior::kDefault;
  keyboard_ui_controller_.UpdateKeyboardConfig(config);
  EXPECT_TRUE(keyboard_ui_controller_.IsKeyboardOverscrollEnabled());

  // Set keyboard_locked() to true.
  keyboard_ui_controller_.set_keyboard_locked(true);
  EXPECT_TRUE(keyboard_ui_controller_.keyboard_locked());
  EXPECT_FALSE(keyboard_ui_controller_.IsKeyboardOverscrollEnabled());
}

// See https://crbug.com/946358.
TEST_F(KeyboardUtilTest, RebuildsWhenChangingAccessibilityFlag) {
  // Virtual keyboard enabled with compact layout.
  SetTouchKeyboardEnabled(true);

  TestKeyboardControllerObserver observer;
  keyboard_ui_controller_.AddObserver(&observer);

  // Virtual keyboard should rebuild to switch to a11y layout.
  SetAccessibilityKeyboardEnabled(true);
  EXPECT_EQ(1, observer.disabled_count);
  EXPECT_EQ(1, observer.enabled_count);

  // Virtual keyboard should rebuild to switch back to compact layout.
  SetAccessibilityKeyboardEnabled(false);
  EXPECT_EQ(2, observer.disabled_count);
  EXPECT_EQ(2, observer.enabled_count);

  keyboard_ui_controller_.RemoveObserver(&observer);
}

}  // namespace keyboard

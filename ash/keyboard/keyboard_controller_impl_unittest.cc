// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/keyboard_controller_impl.h"

#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/keyboard/ui/container_behavior.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/test/test_keyboard_controller_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/wm/core/window_util.h"

using keyboard::KeyboardConfig;
using keyboard::KeyboardEnableFlag;

namespace ash {

namespace {

ShelfLayoutManager* GetShelfLayoutManager() {
  return AshTestBase::GetPrimaryShelf()->shelf_layout_manager();
}

class TestContainerBehavior : public keyboard::ContainerBehavior {
 public:
  TestContainerBehavior() : keyboard::ContainerBehavior(nullptr) {}
  ~TestContainerBehavior() override = default;

  // keyboard::ContainerBehavior
  void DoShowingAnimation(
      aura::Window* window,
      ui::ScopedLayerAnimationSettings* animation_settings) override {}

  void DoHidingAnimation(
      aura::Window* window,
      wm::ScopedHidingAnimationSettings* animation_settings) override {}

  void InitializeShowAnimationStartingState(aura::Window* container) override {}

  gfx::Rect AdjustSetBoundsRequest(
      const gfx::Rect& display_bounds,
      const gfx::Rect& requested_bounds_in_screen_coords) override {
    return adjusted_bounds_in_screen_ ? *adjusted_bounds_in_screen_
                                      : requested_bounds_in_screen_coords;
  }

  void SetCanonicalBounds(aura::Window* container,
                          const gfx::Rect& display_bounds) override {}

  bool IsOverscrollAllowed() const override { return true; }

  void SavePosition(const gfx::Rect& keyboard_bounds_in_screen,
                    const gfx::Size& screen_size) override {}

  bool HandlePointerEvent(const ui::LocatedEvent& event,
                          const display::Display& current_display) override {
    return false;
  }
  bool HandleGestureEvent(const ui::GestureEvent& event,
                          const gfx::Rect& bounds_in_screen) override {
    return false;
  }

  keyboard::ContainerType GetType() const override { return type_; }

  bool TextBlurHidesKeyboard() const override { return false; }

  void SetOccludedBounds(const gfx::Rect& bounds) override {
    occluded_bounds_ = bounds;
  }

  gfx::Rect GetOccludedBounds(
      const gfx::Rect& visual_bounds_in_window) const override {
    return occluded_bounds_;
  }

  bool OccludedBoundsAffectWorkspaceLayout() const override { return false; }

  void SetDraggableArea(const gfx::Rect& rect) override {
    draggable_area_ = rect;
  }

  void SetAreaToRemainOnScreen(const gfx::Rect& rect) override {
    area_to_remain_on_screen_ = rect;
  }

  const gfx::Rect& occluded_bounds() const { return occluded_bounds_; }
  const gfx::Rect& draggable_area() const { return draggable_area_; }
  const gfx::Rect& area_to_remain_on_screen() const {
    return area_to_remain_on_screen_;
  }

  void set_adjusted_bounds_in_screen(const gfx::Rect& rect) {
    adjusted_bounds_in_screen_ = rect;
  }

 private:
  keyboard::ContainerType type_ = keyboard::ContainerType::kFullWidth;
  gfx::Rect occluded_bounds_;
  gfx::Rect draggable_area_;
  gfx::Rect area_to_remain_on_screen_;
  std::optional<gfx::Rect> adjusted_bounds_in_screen_;
};

class KeyboardControllerImplTest : public AshTestBase {
 public:
  KeyboardControllerImplTest() = default;

  KeyboardControllerImplTest(const KeyboardControllerImplTest&) = delete;
  KeyboardControllerImplTest& operator=(const KeyboardControllerImplTest&) =
      delete;

  ~KeyboardControllerImplTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // Set the initial observer config to the default config.
    test_observer()->set_config(keyboard_controller()->GetKeyboardConfig());
  }

  void TearDown() override { AshTestBase::TearDown(); }

  KeyboardControllerImpl* keyboard_controller() {
    return Shell::Get()->keyboard_controller();
  }
  keyboard::KeyboardUIController* keyboard_ui_controller() {
    return keyboard_controller()->keyboard_ui_controller();
  }
  TestKeyboardControllerObserver* test_observer() {
    return ash_test_helper()->test_keyboard_controller_observer();
  }

 protected:
  bool SetContainerType(keyboard::ContainerType container_type,
                        const gfx::Rect& target_bounds) {
    bool result = false;
    base::RunLoop run_loop;
    keyboard_controller()->SetContainerType(
        container_type, target_bounds,
        base::BindLambdaForTesting([&](bool success) {
          result = success;
          run_loop.QuitWhenIdle();
        }));
    run_loop.Run();
    return result;
  }

  aura::Window* GetPrimaryRootWindow() { return Shell::GetPrimaryRootWindow(); }

  aura::Window* GetSecondaryRootWindow() {
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();
    return root_windows[0] == GetPrimaryRootWindow() ? root_windows[1]
                                                     : root_windows[0];
  }

  void CreateFocusedTestWindowInRootWindow(aura::Window* root_window) {
    // Owned by |root_window|.
    aura::Window* focusable_window =
        CreateTestWindowInShellWithBounds(root_window->GetBoundsInScreen());
    focusable_window->Focus();
  }

  void SetKeyboardConfigToPref(const base::Value& value) {
    auto features = base::Value::Dict()
                        .Set("auto_complete_enabled", value.Clone())
                        .Set("auto_correct_enabled", value.Clone())
                        .Set("handwriting_enabled", value.Clone())
                        .Set("spell_check_enabled", value.Clone())
                        .Set("voice_input_enabled", value.Clone());
    PrefService* prefs =
        Shell::Get()->session_controller()->GetLastActiveUserPrefService();
    prefs->SetDict(prefs::kAccessibilityVirtualKeyboardFeatures,
                   std::move(features));
  }

  void VerifyKeyboardConfig(const KeyboardConfig& config, bool expected_value) {
    EXPECT_EQ(test_observer()->config().auto_complete, expected_value);
    EXPECT_EQ(test_observer()->config().auto_correct, expected_value);
    EXPECT_EQ(test_observer()->config().handwriting, expected_value);
    EXPECT_EQ(test_observer()->config().spell_check, expected_value);
    EXPECT_EQ(test_observer()->config().voice_input, expected_value);
  }
};

}  // namespace

TEST_F(KeyboardControllerImplTest, SetKeyboardConfig) {
  // Enable the keyboard so that config changes trigger observer events.
  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  keyboard_controller()->GetKeyboardConfig();
  KeyboardConfig config = keyboard_controller()->GetKeyboardConfig();
  // Set the observer config to the default config.
  test_observer()->set_config(config);

  // Change the keyboard config.
  bool old_auto_complete = config.auto_complete;
  config.auto_complete = !config.auto_complete;
  keyboard_controller()->SetKeyboardConfig(std::move(config));

  // Test that the config changes.
  keyboard_controller()->GetKeyboardConfig();
  EXPECT_NE(old_auto_complete,
            keyboard_controller()->GetKeyboardConfig().auto_complete);

  // Test that the test observer received the change.
  EXPECT_NE(old_auto_complete, test_observer()->config().auto_complete);
}

TEST_F(KeyboardControllerImplTest, SetKeyboardConfigFromPref) {
  // Enable the keyboard so that config changes trigger observer events.
  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  // Set the policy for virtual keyboard features.
  SetKeyboardConfigToPref(base::Value(false));

  // Test that all virtual keyboard features are enabled by default.
  VerifyKeyboardConfig(keyboard_controller()->GetKeyboardConfig(), true);
  VerifyKeyboardConfig(test_observer()->config(), true);

  // Enabled the keyboard config from pref service.
  keyboard_controller()->SetKeyboardConfigFromPref(true);
  VerifyKeyboardConfig(keyboard_controller()->GetKeyboardConfig(), false);
  VerifyKeyboardConfig(test_observer()->config(), false);

  // Change the feature values from 'false' to 'true'.
  SetKeyboardConfigToPref(base::Value(true));
  VerifyKeyboardConfig(keyboard_controller()->GetKeyboardConfig(), true);
  VerifyKeyboardConfig(test_observer()->config(), true);
}

TEST_F(KeyboardControllerImplTest,
       SetKeyboardConfigFromPref_DefaultPolicyValue) {
  // Enable the keyboard so that config changes trigger observer events.
  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  // Enabled the keyboard config from pref service. The feature values should be
  // false by default.
  keyboard_controller()->SetKeyboardConfigFromPref(true);
  VerifyKeyboardConfig(keyboard_controller()->GetKeyboardConfig(), false);
  VerifyKeyboardConfig(test_observer()->config(), false);
}

TEST_F(KeyboardControllerImplTest,
       SetKeyboardConfigFromPref_DefaultFeatureValue) {
  // Enable the keyboard so that config changes trigger observer events.
  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  // Set the policy for virtual keyboard features.
  base::Value features(base::Value::Type::DICT);
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  prefs->Set(prefs::kAccessibilityVirtualKeyboardFeatures, features);

  // Enabled the keyboard config from pref service. The feature values should be
  // false by default.
  keyboard_controller()->SetKeyboardConfigFromPref(true);
  VerifyKeyboardConfig(keyboard_controller()->GetKeyboardConfig(), false);
  VerifyKeyboardConfig(test_observer()->config(), false);
}

TEST_F(KeyboardControllerImplTest,
       SetKeyboardConfigFromPref_InvalidFeatureType) {
  // Enable the keyboard so that config changes trigger observer events.
  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  // The Integer type is not acceptable to virtual keyboard features, so they
  // fallback to use the default value (false).
  SetKeyboardConfigToPref(base::Value(100));
  keyboard_controller()->SetKeyboardConfigFromPref(true);
  VerifyKeyboardConfig(keyboard_controller()->GetKeyboardConfig(), false);
  VerifyKeyboardConfig(test_observer()->config(), false);
}

TEST_F(KeyboardControllerImplTest, EnableFlags) {
  EXPECT_FALSE(keyboard_controller()->IsKeyboardEnabled());
  // Enable the keyboard.
  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
  std::set<keyboard::KeyboardEnableFlag> enable_flags =
      keyboard_controller()->GetEnableFlags();
  EXPECT_TRUE(
      base::Contains(enable_flags, KeyboardEnableFlag::kExtensionEnabled));
  EXPECT_EQ(enable_flags, test_observer()->enable_flags());
  EXPECT_TRUE(keyboard_controller()->IsKeyboardEnabled());

  // Set the enable override to disable the keyboard.
  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kPolicyDisabled);
  enable_flags = keyboard_controller()->GetEnableFlags();
  EXPECT_TRUE(
      base::Contains(enable_flags, KeyboardEnableFlag::kExtensionEnabled));
  EXPECT_TRUE(
      base::Contains(enable_flags, KeyboardEnableFlag::kPolicyDisabled));
  EXPECT_EQ(enable_flags, test_observer()->enable_flags());
  EXPECT_FALSE(keyboard_controller()->IsKeyboardEnabled());

  // Clear the enable override; should enable the keyboard.
  keyboard_controller()->ClearEnableFlag(KeyboardEnableFlag::kPolicyDisabled);
  enable_flags = keyboard_controller()->GetEnableFlags();
  EXPECT_TRUE(
      base::Contains(enable_flags, KeyboardEnableFlag::kExtensionEnabled));
  EXPECT_FALSE(
      base::Contains(enable_flags, KeyboardEnableFlag::kPolicyDisabled));
  EXPECT_EQ(enable_flags, test_observer()->enable_flags());
  EXPECT_TRUE(keyboard_controller()->IsKeyboardEnabled());
}

TEST_F(KeyboardControllerImplTest, RebuildKeyboardIfEnabled) {
  EXPECT_EQ(0, test_observer()->destroyed_count());

  // Enable the keyboard.
  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
  EXPECT_EQ(0, test_observer()->destroyed_count());

  // Enable the keyboard again; this should not reload the keyboard.
  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
  EXPECT_EQ(0, test_observer()->destroyed_count());

  // Rebuild the keyboard. This should destroy the previous keyboard window.
  keyboard_controller()->RebuildKeyboardIfEnabled();
  EXPECT_EQ(1, test_observer()->destroyed_count());

  // Disable the keyboard. The keyboard window should be destroyed.
  keyboard_controller()->ClearEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
  EXPECT_EQ(2, test_observer()->destroyed_count());
}

TEST_F(KeyboardControllerImplTest, ShowAndHideKeyboard) {
  // Enable the keyboard. This will create the keyboard window but not show it.
  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  ASSERT_TRUE(keyboard_ui_controller()->GetKeyboardWindow());
  EXPECT_FALSE(keyboard_ui_controller()->GetKeyboardWindow()->IsVisible());

  // The keyboard needs to be in a loaded state before being shown.
  ASSERT_TRUE(keyboard::test::WaitUntilLoaded());

  keyboard_controller()->ShowKeyboard();
  EXPECT_TRUE(keyboard_ui_controller()->GetKeyboardWindow()->IsVisible());

  keyboard_controller()->HideKeyboard(HideReason::kUser);
  EXPECT_FALSE(keyboard_ui_controller()->GetKeyboardWindow()->IsVisible());

  // TODO(stevenjb): Also use TestKeyboardControllerObserver and
  // IsKeyboardVisible to test visibility changes. https://crbug.com/849995.
}

TEST_F(KeyboardControllerImplTest, SetContainerType) {
  // Enable the keyboard.
  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
  const auto default_behavior = keyboard::ContainerType::kFullWidth;
  EXPECT_EQ(default_behavior,
            keyboard_ui_controller()->GetActiveContainerType());

  gfx::Rect target_bounds(0, 0, 10, 10);
  // Set the container type to kFloating.
  EXPECT_TRUE(
      SetContainerType(keyboard::ContainerType::kFloating, target_bounds));
  EXPECT_EQ(keyboard::ContainerType::kFloating,
            keyboard_ui_controller()->GetActiveContainerType());
  // Ensure that the window size is correct (position is determined by Ash).
  EXPECT_EQ(
      target_bounds.size(),
      keyboard_ui_controller()->GetKeyboardWindow()->GetTargetBounds().size());

  // Setting the container type to the current type should fail.
  EXPECT_FALSE(
      SetContainerType(keyboard::ContainerType::kFloating, target_bounds));
  EXPECT_EQ(keyboard::ContainerType::kFloating,
            keyboard_ui_controller()->GetActiveContainerType());
}

TEST_F(KeyboardControllerImplTest, SetKeyboardLocked) {
  ASSERT_FALSE(keyboard_ui_controller()->keyboard_locked());
  keyboard_controller()->SetKeyboardLocked(true);
  EXPECT_TRUE(keyboard_ui_controller()->keyboard_locked());
  keyboard_controller()->SetKeyboardLocked(false);
  EXPECT_FALSE(keyboard_ui_controller()->keyboard_locked());
}

TEST_F(KeyboardControllerImplTest, SetOccludedBounds) {
  // Enable the keyboard.
  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  // Override the container behavior.
  auto scoped_behavior = std::make_unique<TestContainerBehavior>();
  TestContainerBehavior* behavior = scoped_behavior.get();
  keyboard_ui_controller()->set_container_behavior_for_test(
      std::move(scoped_behavior));

  gfx::Rect bounds(10, 20, 30, 40);
  keyboard_controller()->SetOccludedBounds({bounds});
  EXPECT_EQ(bounds, behavior->occluded_bounds());
}

TEST_F(KeyboardControllerImplTest, SetHitTestBounds) {
  // Enable the keyboard.
  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
  ASSERT_FALSE(keyboard_ui_controller()->GetKeyboardWindow()->targeter());

  // Setting the hit test bounds should set a WindowTargeter.
  keyboard_ui_controller()->SetHitTestBounds({gfx::Rect(10, 20, 30, 40)});
  ASSERT_TRUE(keyboard_ui_controller()->GetKeyboardWindow()->targeter());
}

TEST_F(KeyboardControllerImplTest, SetDraggableArea) {
  // Enable the keyboard.
  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  // Override the container behavior.
  auto scoped_behavior = std::make_unique<TestContainerBehavior>();
  TestContainerBehavior* behavior = scoped_behavior.get();
  keyboard_ui_controller()->set_container_behavior_for_test(
      std::move(scoped_behavior));

  gfx::Rect bounds(10, 20, 30, 40);
  keyboard_ui_controller()->SetDraggableArea(bounds);
  EXPECT_EQ(bounds, behavior->draggable_area());
}

TEST_F(KeyboardControllerImplTest, SetAreaToRemainOnScreen) {
  // Enable the keyboard.
  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  // Override the container behavior.
  auto scoped_behavior = std::make_unique<TestContainerBehavior>();
  TestContainerBehavior* behavior = scoped_behavior.get();
  keyboard_ui_controller()->set_container_behavior_for_test(
      std::move(scoped_behavior));

  gfx::Rect bounds(10, 20, 30, 40);
  keyboard_ui_controller()->SetAreaToRemainOnScreen(bounds);
  EXPECT_EQ(bounds, behavior->area_to_remain_on_screen());
}

TEST_F(KeyboardControllerImplTest, SetWindowBoundsInScreen) {
  // Enable the keyboard.
  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  // Override the container behavior.
  auto scoped_behavior = std::make_unique<TestContainerBehavior>();
  keyboard_ui_controller()->set_container_behavior_for_test(
      std::move(scoped_behavior));

  gfx::Rect bounds(1, 1, 300, 400);
  keyboard_controller()->SetWindowBoundsInScreen(bounds);
  EXPECT_EQ(bounds,
            keyboard_ui_controller()->GetKeyboardWindow()->GetBoundsInScreen());
}

TEST_F(KeyboardControllerImplTest,
       SetWindowBoundsInScreenShouldRespectAdjustedBounds) {
  gfx::Rect adjusted_bounds_in_screen(10, 10, 30, 40);

  // Enable the keyboard.
  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  // Override the container behavior.
  auto scoped_behavior = std::make_unique<TestContainerBehavior>();
  scoped_behavior->set_adjusted_bounds_in_screen(adjusted_bounds_in_screen);
  keyboard_ui_controller()->set_container_behavior_for_test(
      std::move(scoped_behavior));

  gfx::Rect requested_bounds(1, 1, 300, 400);
  keyboard_controller()->SetWindowBoundsInScreen(requested_bounds);
  EXPECT_EQ(adjusted_bounds_in_screen,
            keyboard_ui_controller()->GetKeyboardWindow()->GetBoundsInScreen());
}

TEST_F(KeyboardControllerImplTest, ChangingSessionRebuildsKeyboard) {
  // Enable the keyboard.
  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  // LOGGED_IN_NOT_ACTIVE session state needs to rebuild keyboard for supervised
  // user profile.
  Shell::Get()->keyboard_controller()->OnSessionStateChanged(
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  EXPECT_EQ(1, test_observer()->destroyed_count());

  // ACTIVE session state also needs to rebuild keyboard for guest user profile.
  Shell::Get()->keyboard_controller()->OnSessionStateChanged(
      session_manager::SessionState::ACTIVE);
  EXPECT_EQ(2, test_observer()->destroyed_count());
}

TEST_F(KeyboardControllerImplTest, VisualBoundsInMultipleDisplays) {
  UpdateDisplay("800x600,1024x768");

  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  // Show the keyboard in the second display.
  keyboard_ui_controller()->ShowKeyboardInDisplay(
      display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
          .GetSecondaryDisplay());
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  gfx::Rect root_bounds = keyboard_ui_controller()->visual_bounds_in_root();
  EXPECT_EQ(0, root_bounds.x());

  gfx::Rect screen_bounds = keyboard_ui_controller()->GetVisualBoundsInScreen();
  EXPECT_EQ(800, screen_bounds.x());
}

TEST_F(KeyboardControllerImplTest, OccludedBoundsInMultipleDisplays) {
  UpdateDisplay("800x600,1024x768");

  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  // Show the keyboard in the second display.
  keyboard_ui_controller()->ShowKeyboardInDisplay(
      display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
          .GetSecondaryDisplay());
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  gfx::Rect screen_bounds =
      keyboard_ui_controller()->GetWorkspaceOccludedBoundsInScreen();
  EXPECT_EQ(800, screen_bounds.x());
}

// Test for http://crbug.com/303429. |GetContainerForDisplay| should move
// keyboard to specified display even when it's not touchable.
TEST_F(KeyboardControllerImplTest, GetContainerForDisplay) {
  UpdateDisplay("600x500,600x500");

  // Make primary display touchable.
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .SetTouchSupport(GetPrimaryDisplay().id(),
                       display::Display::TouchSupport::AVAILABLE);

  EXPECT_EQ(display::Display::TouchSupport::AVAILABLE,
            GetPrimaryDisplay().touch_support());
  EXPECT_NE(display::Display::TouchSupport::AVAILABLE,
            GetSecondaryDisplay().touch_support());

  // Move to primary display.
  EXPECT_EQ(GetPrimaryRootWindow(),
            keyboard_controller()
                ->GetContainerForDisplay(GetPrimaryDisplay())
                ->GetRootWindow());

  // Move to secondary display.
  EXPECT_EQ(GetSecondaryRootWindow(),
            keyboard_controller()
                ->GetContainerForDisplay(GetSecondaryDisplay())
                ->GetRootWindow());
}

// Test for http://crbug.com/297858. |GetContainerForDefaultDisplay| should
// return the primary display if no display has touch capability and
// no window is focused.
TEST_F(KeyboardControllerImplTest,
       DefaultContainerInPrimaryDisplayWhenNoDisplayHasTouch) {
  UpdateDisplay("600x500,600x500");

  EXPECT_NE(display::Display::TouchSupport::AVAILABLE,
            GetPrimaryDisplay().touch_support());
  EXPECT_NE(display::Display::TouchSupport::AVAILABLE,
            GetSecondaryDisplay().touch_support());

  EXPECT_EQ(
      GetPrimaryRootWindow(),
      keyboard_controller()->GetContainerForDefaultDisplay()->GetRootWindow());
}

// Test for http://crbug.com/297858. |GetContainerForDefaultDisplay| should
// move keyboard to focused display if no display has touch capability.
TEST_F(KeyboardControllerImplTest,
       DefaultContainerIsInFocusedDisplayWhenNoDisplayHasTouch) {
  UpdateDisplay("600x500,600x500");

  EXPECT_NE(display::Display::TouchSupport::AVAILABLE,
            GetPrimaryDisplay().touch_support());
  EXPECT_NE(display::Display::TouchSupport::AVAILABLE,
            GetSecondaryDisplay().touch_support());

  CreateFocusedTestWindowInRootWindow(GetSecondaryRootWindow());
  EXPECT_EQ(
      GetSecondaryRootWindow(),
      keyboard_controller()->GetContainerForDefaultDisplay()->GetRootWindow());
}

// Test for http://crbug.com/303429. |GetContainerForDefaultDisplay| should
// move keyboard to first touchable display when there is one.
TEST_F(KeyboardControllerImplTest, DefaultContainerIsInFirstTouchableDisplay) {
  UpdateDisplay("600x500,600x500");

  // Make secondary display touchable.
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .SetTouchSupport(GetSecondaryDisplay().id(),
                       display::Display::TouchSupport::AVAILABLE);

  EXPECT_NE(display::Display::TouchSupport::AVAILABLE,
            GetPrimaryDisplay().touch_support());
  EXPECT_EQ(display::Display::TouchSupport::AVAILABLE,
            GetSecondaryDisplay().touch_support());

  EXPECT_EQ(
      GetSecondaryRootWindow(),
      keyboard_controller()->GetContainerForDefaultDisplay()->GetRootWindow());
}

// Test for http://crbug.com/303429. |GetContainerForDefaultDisplay| should
// move keyboard to first touchable display when the focused display is not
// touchable.
TEST_F(
    KeyboardControllerImplTest,
    DefaultContainerIsInFirstTouchableDisplayIfFocusedDisplayIsNotTouchable) {
  UpdateDisplay("600x500,600x500");

  // Make secondary display touchable.
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .SetTouchSupport(GetSecondaryDisplay().id(),
                       display::Display::TouchSupport::AVAILABLE);

  EXPECT_NE(display::Display::TouchSupport::AVAILABLE,
            GetPrimaryDisplay().touch_support());
  EXPECT_EQ(display::Display::TouchSupport::AVAILABLE,
            GetSecondaryDisplay().touch_support());

  // Focus on primary display.
  CreateFocusedTestWindowInRootWindow(GetPrimaryRootWindow());

  EXPECT_EQ(
      GetSecondaryRootWindow(),
      keyboard_controller()->GetContainerForDefaultDisplay()->GetRootWindow());
}

// Test for http://crbug.com/303429. |GetContainerForDefaultDisplay| should
// move keyborad to first touchable display when there is one.
TEST_F(KeyboardControllerImplTest,
       DefaultContainerIsInFocusedDisplayIfTouchable) {
  UpdateDisplay("600x500,600x500");

  // Make both displays touchable.
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .SetTouchSupport(GetPrimaryDisplay().id(),
                       display::Display::TouchSupport::AVAILABLE);
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .SetTouchSupport(GetSecondaryDisplay().id(),
                       display::Display::TouchSupport::AVAILABLE);

  EXPECT_EQ(display::Display::TouchSupport::AVAILABLE,
            GetPrimaryDisplay().touch_support());
  EXPECT_EQ(display::Display::TouchSupport::AVAILABLE,
            GetSecondaryDisplay().touch_support());

  // Focus on secondary display.
  CreateFocusedTestWindowInRootWindow(GetSecondaryRootWindow());
  EXPECT_EQ(
      GetSecondaryRootWindow(),
      keyboard_controller()->GetContainerForDefaultDisplay()->GetRootWindow());

  // Focus on primary display.
  CreateFocusedTestWindowInRootWindow(GetPrimaryRootWindow());
  EXPECT_EQ(
      GetPrimaryRootWindow(),
      keyboard_controller()->GetContainerForDefaultDisplay()->GetRootWindow());
}

// Test for https://crbug.com/897007.
TEST_F(KeyboardControllerImplTest, ShowKeyboardInSecondaryDisplay) {
  UpdateDisplay("600x500,600x500");

  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  // Show in secondary display.
  keyboard_ui_controller()->ShowKeyboardInDisplay(GetSecondaryDisplay());
  EXPECT_EQ(GetSecondaryRootWindow(),
            keyboard_ui_controller()->GetRootWindow());
  ASSERT_TRUE(keyboard::test::WaitUntilShown());
  EXPECT_TRUE(
      !keyboard_ui_controller()->GetKeyboardWindow()->bounds().IsEmpty());
}

TEST_F(KeyboardControllerImplTest, SwipeUpToShowHotSeat) {
  TabletModeControllerTestApi().EnterTabletMode();

  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  keyboard_ui_controller()->ShowKeyboard(/* lock */ false);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  const gfx::Point start(display_bounds.bottom_center());
  const gfx::Point end(start + gfx::Vector2d(0, -80));
  const base::TimeDelta time_delta = base::Milliseconds(100);
  const int num_scroll_steps = 4;
  GetEventGenerator()->GestureScrollSequence(start, end, time_delta,
                                             num_scroll_steps);

  // Keyboard should hide and gesture should forward to the shelf.
  ASSERT_TRUE(keyboard::test::WaitUntilHidden());
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
}

TEST_F(KeyboardControllerImplTest, FlingUpToShowOverviewMode) {
  TabletModeControllerTestApi().EnterTabletMode();

  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  keyboard_ui_controller()->ShowKeyboard(/* lock */ false);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  const gfx::Point start(display_bounds.bottom_center());
  const gfx::Point end(start + gfx::Vector2d(0, -200));
  const int fling_speed =
      DragWindowFromShelfController::kVelocityToHomeScreenThreshold + 1;
  const int scroll_steps = 20;
  base::TimeDelta scroll_time =
      GetEventGenerator()->CalculateScrollDurationForFlingVelocity(
          start, end, fling_speed, scroll_steps);
  GetEventGenerator()->GestureScrollSequence(start, end, scroll_time,
                                             scroll_steps);

  // Keyboard should hide and gesture should forward to the shelf.
  ASSERT_TRUE(keyboard::test::WaitUntilHidden());
  EXPECT_EQ(HotseatState::kShownHomeLauncher,
            GetShelfLayoutManager()->hotseat_state());
}

TEST_F(KeyboardControllerImplTest, SwipeUpDoesntHideKeyboardInClamshellMode) {
  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  keyboard_controller()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  keyboard_ui_controller()->ShowKeyboard(/* lock */ false);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  const gfx::Point start(display_bounds.bottom_center());
  const gfx::Point end(start + gfx::Vector2d(0, -80));
  const base::TimeDelta time_delta = base::Milliseconds(100);
  const int num_scroll_steps = 4;
  GetEventGenerator()->GestureScrollSequence(start, end, time_delta,
                                             num_scroll_steps);

  EXPECT_FALSE(keyboard::test::IsKeyboardHiding());
}

TEST_F(KeyboardControllerImplTest, RecordsKeyRepeatSettings) {
  // Initially expect no user preferences recorded.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.KeyboardAutoRepeatDelay", /*count=*/0u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.KeyboardAutoRepeatEnabled", /*count=*/0u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.KeyboardAutoRepeatInterval", /*count=*/0u);

  SimulateUserLogin("user1");

  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.KeyboardAutoRepeatDelay", /*count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.KeyboardAutoRepeatEnabled", /*count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.KeyboardAutoRepeatInterval", /*count=*/1u);

  SimulateUserLogin("user2");

  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.KeyboardAutoRepeatDelay", /*count=*/2u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.KeyboardAutoRepeatEnabled", /*count=*/2u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.KeyboardAutoRepeatInterval", /*count=*/2u);

  SimulateUserLogin("user1");

  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.KeyboardAutoRepeatDelay", /*count=*/2u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.KeyboardAutoRepeatEnabled", /*count=*/2u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.KeyboardAutoRepeatInterval", /*count=*/2u);
}

}  // namespace ash

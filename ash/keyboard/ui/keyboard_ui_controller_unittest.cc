// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/keyboard_ui_controller.h"

#include <memory>

#include "ash/keyboard/ui/container_full_width_behavior.h"
#include "ash/keyboard/ui/keyboard_layout_manager.h"
#include "ash/keyboard/ui/keyboard_ui.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/keyboard/ui/test/test_keyboard_layout_delegate.h"
#include "ash/keyboard/ui/test/test_keyboard_ui_factory.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/default_screen_position_client.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace keyboard {
namespace {

// Steps a layer animation until it is completed. Animations must be enabled.
void RunAnimationForLayer(ui::Layer* layer) {
  // Animations must be enabled for stepping to work.
  ASSERT_NE(ui::ScopedAnimationDurationScaleMode::duration_scale_mode(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  ui::LayerAnimatorTestController controller(layer->GetAnimator());
  // Multiple steps are required to complete complex animations.
  // TODO(vollick): This should not be necessary. crbug.com/154017
  while (controller.animator()->is_animating()) {
    controller.StartThreadedAnimationsIfNeeded();
    base::TimeTicks step_time = controller.animator()->last_step_time();
    controller.animator()->Step(step_time +
                                base::TimeDelta::FromMilliseconds(1000));
  }
}

// An event handler that focuses a window when it is clicked/touched on. This is
// used to match the focus manger behaviour in ash and views.
class TestFocusController : public ui::EventHandler {
 public:
  explicit TestFocusController(aura::Window* root) : root_(root) {
    root_->AddPreTargetHandler(this);
  }

  ~TestFocusController() override { root_->RemovePreTargetHandler(this); }

 private:
  // Overridden from ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    auto* target = static_cast<aura::Window*>(event->target());
    if (event->type() == ui::ET_MOUSE_PRESSED ||
        event->type() == ui::ET_TOUCH_PRESSED) {
      aura::client::GetFocusClient(target)->FocusWindow(target);
    }
  }

  aura::Window* root_;
  DISALLOW_COPY_AND_ASSIGN(TestFocusController);
};

class KeyboardContainerObserver : public aura::WindowObserver {
 public:
  explicit KeyboardContainerObserver(aura::Window* window,
                                     base::RunLoop* run_loop)
      : window_(window), run_loop_(run_loop) {
    window_->AddObserver(this);
  }
  ~KeyboardContainerObserver() override { window_->RemoveObserver(this); }

 private:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    if (!visible)
      run_loop_->QuitWhenIdle();
  }

  aura::Window* window_;
  base::RunLoop* const run_loop_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardContainerObserver);
};

class SetModeCallbackInvocationCounter {
 public:
  SetModeCallbackInvocationCounter() {}

  void Invoke(bool status) {
    if (status)
      invocation_count_success_++;
    else
      invocation_count_failure_++;
  }

  base::OnceCallback<void(bool)> GetInvocationCallback() {
    return base::BindOnce(&SetModeCallbackInvocationCounter::Invoke,
                          weak_factory_invoke_.GetWeakPtr());
  }

  int invocation_count_for_status(bool status) {
    return status ? invocation_count_success_ : invocation_count_failure_;
  }

 private:
  int invocation_count_success_ = 0;
  int invocation_count_failure_ = 0;
  base::WeakPtrFactory<SetModeCallbackInvocationCounter> weak_factory_invoke_{
      this};
};

}  // namespace

class KeyboardUIControllerTest : public aura::test::AuraTestBase,
                                 public ash::KeyboardControllerObserver {
 public:
  KeyboardUIControllerTest() = default;
  ~KeyboardUIControllerTest() override = default;

  void SetUp() override {
    aura::test::AuraTestBase::SetUp();
    new wm::DefaultActivationClient(root_window());
    focus_controller_ = std::make_unique<TestFocusController>(root_window());
    layout_delegate_ =
        std::make_unique<TestKeyboardLayoutDelegate>(root_window());

    aura::client::SetScreenPositionClient(root_window(),
                                          &screen_position_client_);

    // Force enable the virtual keyboard.
    controller_.Initialize(
        std::make_unique<TestKeyboardUIFactory>(host()->GetInputMethod()),
        layout_delegate_.get());
    keyboard::SetTouchKeyboardEnabled(true);
    controller_.AddObserver(this);
  }

  void TearDown() override {
    keyboard::SetTouchKeyboardEnabled(false);
    controller_.RemoveObserver(this);
    focus_controller_.reset();
    aura::test::AuraTestBase::TearDown();
  }

  KeyboardUIController& controller() { return controller_; }
  KeyboardLayoutDelegate* layout_delegate() { return layout_delegate_.get(); }

  void ShowKeyboard() {
    test_text_input_client_ =
        std::make_unique<ui::DummyTextInputClient>(ui::TEXT_INPUT_TYPE_TEXT);
    SetFocus(test_text_input_client_.get());
  }

  void MockRotateScreen() {
    const gfx::Rect root_bounds = root_window()->bounds();
    root_window()->SetBounds(
        gfx::Rect(0, 0, root_bounds.height(), root_bounds.width()));
  }

 protected:
  // KeyboardControllerObserver overrides
  void OnKeyboardVisibleBoundsChanged(const gfx::Rect& new_bounds) override {
    visible_bounds_ = new_bounds;
    visible_bounds_number_of_calls_++;
  }
  void OnKeyboardOccludedBoundsChanged(const gfx::Rect& new_bounds) override {
    occluding_bounds_ = new_bounds;
    occluding_bounds_number_of_calls_++;
  }
  void OnKeyboardVisibilityChanged(bool is_visible) override {
    is_visible_ = is_visible;
    is_visible_number_of_calls_++;
  }
  void OnKeyboardEnabledChanged(bool is_enabled) override {
    keyboard_disabled_ = !is_enabled;
  }
  void ClearKeyboardDisabled() { keyboard_disabled_ = false; }

  int visible_bounds_number_of_calls() const {
    return visible_bounds_number_of_calls_;
  }
  int occluding_bounds_number_of_calls() const {
    return occluding_bounds_number_of_calls_;
  }
  int is_visible_number_of_calls() const { return is_visible_number_of_calls_; }

  const gfx::Rect& notified_visible_bounds() { return visible_bounds_; }
  const gfx::Rect& notified_occluding_bounds() { return occluding_bounds_; }
  bool notified_is_visible() { return is_visible_; }

  bool IsKeyboardDisabled() { return keyboard_disabled_; }

  void SetProgrammaticFocus(ui::TextInputClient* client) {
    controller_.OnTextInputStateChanged(client);
  }

  void AddTimeToTransientBlurCounter(double seconds) {
    controller_.time_of_last_blur_ -=
        base::TimeDelta::FromMilliseconds(int{1000 * seconds});
  }

  void SetFocus(ui::TextInputClient* client) {
    ui::InputMethod* input_method = controller().GetInputMethodForTest();
    input_method->SetFocusedTextInputClient(client);
    if (client && client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE &&
        client->GetTextInputMode() != ui::TEXT_INPUT_MODE_NONE) {
      input_method->ShowVirtualKeyboardIfEnabled();
      ASSERT_TRUE(keyboard::WaitUntilShown());
    }
  }

  bool WillHideKeyboard() { return controller_.WillHideKeyboard(); }

  bool ShouldEnableInsets(aura::Window* window) {
    aura::Window* contents_window = controller().GetKeyboardWindow();
    return (contents_window->GetRootWindow() == window->GetRootWindow() &&
            controller_.IsKeyboardOverscrollEnabled() &&
            contents_window->IsVisible() && controller_.IsKeyboardVisible());
  }

  void RunLoop(base::RunLoop* run_loop) {
#if defined(USE_OZONE)
    // TODO(crbug/776357): Figure out why the initializer randomly doesn't run
    // for some tests. In the mean time, prevent flaky Ozone crash.
    ui::OzonePlatform::InitializeForGPU(ui::OzonePlatform::InitParams());
#endif
    run_loop->Run();
  }

  std::unique_ptr<TestFocusController> focus_controller_;

 private:
  int visible_bounds_number_of_calls_ = 0;
  gfx::Rect visible_bounds_;
  int occluding_bounds_number_of_calls_ = 0;
  gfx::Rect occluding_bounds_;
  int is_visible_number_of_calls_ = 0;
  bool is_visible_ = false;

  KeyboardUIController controller_;

  std::unique_ptr<KeyboardLayoutDelegate> layout_delegate_;
  std::unique_ptr<ui::TextInputClient> test_text_input_client_;
  bool keyboard_disabled_ = false;
  wm::DefaultScreenPositionClient screen_position_client_;
  ui::ScopedTestInputMethodFactory scoped_test_input_method_factory_;
  DISALLOW_COPY_AND_ASSIGN(KeyboardUIControllerTest);
};

// TODO(https://crbug.com/849995): This is testing KeyboardLayoutManager /
// ContainerFullWidthBehavior. Put this test there.
TEST_F(KeyboardUIControllerTest, KeyboardSize) {
  root_window()->SetLayoutManager(new KeyboardLayoutManager(&controller()));

  // The keyboard window should not be visible.
  aura::Window* keyboard_window = controller().GetKeyboardWindow();
  EXPECT_FALSE(keyboard_window->IsVisible());

  const gfx::Rect screen_bounds = root_window()->bounds();

  // Attempt to change window width or move window up from the bottom are
  // ignored. Changing window height is supported.
  gfx::Rect expected_keyboard_bounds(0, screen_bounds.height() - 50,
                                     screen_bounds.width(), 50);

  // The x position of new bounds may not be 0 if shelf is on the left side of
  // screen. The virtual keyboard should always align with the left edge of
  // screen. See http://crbug.com/510595.
  gfx::Rect new_bounds(10, 0, 50, 50);
  keyboard_window->SetBounds(new_bounds);
  EXPECT_EQ(expected_keyboard_bounds, keyboard_window->bounds());

  MockRotateScreen();
  // The above call should resize keyboard to new width while keeping the old
  // height.
  EXPECT_EQ(
      gfx::Rect(0, screen_bounds.width() - 50, screen_bounds.height(), 50),
      keyboard_window->bounds());
}

// Tests that blur-then-focus that occur in less than the transient threshold
// cause the keyboard to re-show.
TEST_F(KeyboardUIControllerTest, TransientBlurShortDelay) {
  ui::DummyTextInputClient input_client(ui::TEXT_INPUT_TYPE_TEXT);
  ui::DummyTextInputClient no_input_client(ui::TEXT_INPUT_TYPE_NONE);

  base::RunLoop run_loop;
  aura::Window* keyboard_window = controller().GetKeyboardWindow();
  auto keyboard_container_observer =
      std::make_unique<KeyboardContainerObserver>(keyboard_window, &run_loop);

  // Keyboard is hidden
  EXPECT_FALSE(keyboard_window->IsVisible());

  // Set programmatic focus to the text field. Nothing happens
  SetProgrammaticFocus(&input_client);
  EXPECT_FALSE(keyboard_window->IsVisible());

  // Click it for real. Keyboard starts to appear.
  SetFocus(&input_client);
  EXPECT_TRUE(keyboard_window->IsVisible());

  // Focus a non text field
  SetFocus(&no_input_client);

  // It waits 100 ms and then hides. Wait for this routine to finish.
  EXPECT_TRUE(WillHideKeyboard());
  RunLoop(&run_loop);
  EXPECT_FALSE(keyboard_window->IsVisible());

  // Virtually wait half a second
  AddTimeToTransientBlurCounter(0.5);
  // Apply programmatic focus to the text field.
  SetProgrammaticFocus(&input_client);

  // TODO(blakeo): this is not technically wrong, but the DummyTextInputClient
  // should allow for overriding the text input flags, to simulate testing
  // a web-based text field.
  EXPECT_FALSE(keyboard_window->IsVisible());
  EXPECT_FALSE(WillHideKeyboard());
}

// Tests that blur-then-focus that occur past the transient threshold do not
// cause the keyboard to re-show.
TEST_F(KeyboardUIControllerTest, TransientBlurLongDelay) {
  ui::DummyTextInputClient input_client(ui::TEXT_INPUT_TYPE_TEXT);
  ui::DummyTextInputClient no_input_client(ui::TEXT_INPUT_TYPE_NONE);

  base::RunLoop run_loop;
  aura::Window* keyboard_window = controller().GetKeyboardWindow();
  auto keyboard_container_observer =
      std::make_unique<KeyboardContainerObserver>(keyboard_window, &run_loop);

  // Keyboard is hidden
  EXPECT_FALSE(keyboard_window->IsVisible());

  // Set programmatic focus to the text field. Nothing happens
  SetProgrammaticFocus(&input_client);
  EXPECT_FALSE(keyboard_window->IsVisible());

  // Click it for real. Keyboard starts to appear.
  SetFocus(&input_client);
  EXPECT_TRUE(keyboard_window->IsVisible());

  // Focus a non text field
  SetFocus(&no_input_client);

  // It waits 100 ms and then hides. Wait for this routine to finish.
  EXPECT_TRUE(WillHideKeyboard());
  RunLoop(&run_loop);
  EXPECT_FALSE(keyboard_window->IsVisible());

  // Wait 5 seconds and then set programmatic focus to a text field
  AddTimeToTransientBlurCounter(5.0);
  SetProgrammaticFocus(&input_client);
  EXPECT_FALSE(keyboard_window->IsVisible());
}

TEST_F(KeyboardUIControllerTest, VisibilityChangeWithTextInputTypeChange) {
  ui::DummyTextInputClient input_client_0(ui::TEXT_INPUT_TYPE_TEXT);
  ui::DummyTextInputClient input_client_1(ui::TEXT_INPUT_TYPE_TEXT);
  ui::DummyTextInputClient input_client_2(ui::TEXT_INPUT_TYPE_TEXT);
  ui::DummyTextInputClient no_input_client_0(ui::TEXT_INPUT_TYPE_NONE);
  ui::DummyTextInputClient no_input_client_1(ui::TEXT_INPUT_TYPE_NONE);

  base::RunLoop run_loop;
  aura::Window* keyboard_window = controller().GetKeyboardWindow();
  auto keyboard_container_observer =
      std::make_unique<KeyboardContainerObserver>(keyboard_window, &run_loop);

  SetFocus(&input_client_0);

  EXPECT_TRUE(keyboard_window->IsVisible());

  SetFocus(&no_input_client_0);
  // Keyboard should not immediately hide itself. It is delayed to avoid layout
  // flicker when the focus of input field quickly change.
  EXPECT_TRUE(keyboard_window->IsVisible());
  EXPECT_TRUE(WillHideKeyboard());
  // Wait for hide keyboard to finish.

  RunLoop(&run_loop);
  EXPECT_FALSE(keyboard_window->IsVisible());

  SetFocus(&input_client_1);
  EXPECT_TRUE(keyboard_window->IsVisible());

  // Schedule to hide keyboard.
  SetFocus(&no_input_client_1);
  EXPECT_TRUE(WillHideKeyboard());
  // Cancel keyboard hide.
  SetFocus(&input_client_2);

  EXPECT_FALSE(WillHideKeyboard());
  EXPECT_TRUE(keyboard_window->IsVisible());
}

// Test to prevent spurious overscroll boxes when changing tabs during keyboard
// hide. Refer to crbug.com/401670 for more context.
TEST_F(KeyboardUIControllerTest, CheckOverscrollInsetDuringVisibilityChange) {
  ui::DummyTextInputClient input_client(ui::TEXT_INPUT_TYPE_TEXT);
  ui::DummyTextInputClient no_input_client(ui::TEXT_INPUT_TYPE_NONE);

  // Enable touch keyboard / overscroll mode to test insets.
  EXPECT_TRUE(controller().IsKeyboardOverscrollEnabled());

  SetFocus(&input_client);
  SetFocus(&no_input_client);
  // Insets should not be enabled for new windows while keyboard is in the
  // process of hiding when overscroll is enabled.
  EXPECT_FALSE(ShouldEnableInsets(controller().GetKeyboardWindow()));
  // Cancel keyboard hide.
  SetFocus(&input_client);
  // Insets should be enabled for new windows as hide was cancelled.
  EXPECT_TRUE(ShouldEnableInsets(controller().GetKeyboardWindow()));
}

TEST_F(KeyboardUIControllerTest, AlwaysVisibleWhenLocked) {
  ui::DummyTextInputClient input_client_0(ui::TEXT_INPUT_TYPE_TEXT);
  ui::DummyTextInputClient input_client_1(ui::TEXT_INPUT_TYPE_TEXT);
  ui::DummyTextInputClient no_input_client_0(ui::TEXT_INPUT_TYPE_NONE);
  ui::DummyTextInputClient no_input_client_1(ui::TEXT_INPUT_TYPE_NONE);

  base::RunLoop run_loop;
  aura::Window* keyboard_window = controller().GetKeyboardWindow();
  auto keyboard_container_observer =
      std::make_unique<KeyboardContainerObserver>(keyboard_window, &run_loop);

  SetFocus(&input_client_0);

  EXPECT_TRUE(keyboard_window->IsVisible());

  // Lock keyboard.
  controller().set_keyboard_locked(true);

  SetFocus(&no_input_client_0);
  // Keyboard should not try to hide itself as it is locked.
  EXPECT_TRUE(keyboard_window->IsVisible());
  EXPECT_FALSE(WillHideKeyboard());

  // Implicit hiding will not do anything when the keyboard is locked.
  controller().HideKeyboardImplicitlyBySystem();
  EXPECT_TRUE(keyboard_window->IsVisible());
  EXPECT_FALSE(WillHideKeyboard());

  SetFocus(&input_client_1);
  EXPECT_TRUE(keyboard_window->IsVisible());

  // Unlock keyboard.
  controller().set_keyboard_locked(false);

  // Keyboard should hide when focus on no input client.
  SetFocus(&no_input_client_1);
  EXPECT_TRUE(WillHideKeyboard());

  // Wait for hide keyboard to finish.
  RunLoop(&run_loop);
  EXPECT_FALSE(keyboard_window->IsVisible());
}

// Tests that disabling the keyboard will get a corresponding event.
TEST_F(KeyboardUIControllerTest, DisableKeyboard) {
  aura::Window* keyboard_window = controller().GetKeyboardWindow();

  ShowKeyboard();
  EXPECT_TRUE(keyboard_window->IsVisible());
  EXPECT_FALSE(IsKeyboardDisabled());

  keyboard::SetTouchKeyboardEnabled(false);
  EXPECT_TRUE(IsKeyboardDisabled());
}

class KeyboardControllerAnimationTest : public KeyboardUIControllerTest {
 public:
  KeyboardControllerAnimationTest() = default;
  ~KeyboardControllerAnimationTest() override = default;

  void SetUp() override {
    // We cannot short-circuit animations for this test.
    ui::ScopedAnimationDurationScaleMode test_duration_mode(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    KeyboardUIControllerTest::SetUp();

    // Wait for the keyboard contents to load.
    base::RunLoop().RunUntilIdle();
    keyboard_window()->SetBounds(root_window()->bounds());
  }

  void TearDown() override { KeyboardUIControllerTest::TearDown(); }

 protected:
  aura::Window* keyboard_window() { return controller().GetKeyboardWindow(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardControllerAnimationTest);
};

TEST_F(KeyboardControllerAnimationTest, ContainerAnimation) {
  ui::Layer* layer = keyboard_window()->layer();
  ShowKeyboard();

  // Keyboard container and window should immediately become visible before
  // animation starts.
  EXPECT_TRUE(keyboard_window()->IsVisible());
  float show_start_opacity = layer->opacity();
  gfx::Transform transform;
  transform.Translate(0, keyboard::kFullWidthKeyboardAnimationDistance);
  EXPECT_EQ(transform, layer->transform());
  // Actual final bounds should be notified after animation finishes to avoid
  // flash of background being seen.
  EXPECT_EQ(gfx::Rect(), notified_visible_bounds());
  EXPECT_EQ(gfx::Rect(), notified_occluding_bounds());
  EXPECT_FALSE(notified_is_visible());

  RunAnimationForLayer(layer);
  EXPECT_TRUE(keyboard_window()->IsVisible());
  float show_end_opacity = layer->opacity();
  EXPECT_LT(show_start_opacity, show_end_opacity);
  EXPECT_EQ(gfx::Transform(), layer->transform());
  // KeyboardController should notify the bounds of container window to its
  // observers after show animation finished.
  EXPECT_EQ(keyboard_window()->bounds(), notified_visible_bounds());
  EXPECT_EQ(keyboard_window()->bounds(), notified_occluding_bounds());
  EXPECT_TRUE(notified_is_visible());

  // Directly hide keyboard without delay.
  float hide_start_opacity = layer->opacity();
  controller().HideKeyboardExplicitlyBySystem();
  EXPECT_FALSE(keyboard_window()->IsVisible());
  EXPECT_FALSE(keyboard_window()->layer()->visible());
  layer = keyboard_window()->layer();
  // KeyboardController should notify the bounds of keyboard window to its
  // observers before hide animation starts.
  EXPECT_EQ(gfx::Rect(), notified_visible_bounds());
  EXPECT_EQ(gfx::Rect(), notified_occluding_bounds());
  EXPECT_FALSE(notified_is_visible());

  RunAnimationForLayer(layer);
  EXPECT_FALSE(keyboard_window()->IsVisible());
  EXPECT_FALSE(keyboard_window()->layer()->visible());
  float hide_end_opacity = layer->opacity();
  EXPECT_GT(hide_start_opacity, hide_end_opacity);
  EXPECT_EQ(gfx::Rect(), notified_visible_bounds());
  EXPECT_EQ(gfx::Rect(), notified_occluding_bounds());
  EXPECT_FALSE(notified_is_visible());

  SetModeCallbackInvocationCounter invocation_counter;
  controller().SetContainerType(ContainerType::kFloating, base::nullopt,
                                invocation_counter.GetInvocationCallback());
  EXPECT_EQ(1, invocation_counter.invocation_count_for_status(true));
  EXPECT_EQ(0, invocation_counter.invocation_count_for_status(false));
  ShowKeyboard();
  RunAnimationForLayer(layer);
  EXPECT_EQ(1, invocation_counter.invocation_count_for_status(true));
  EXPECT_EQ(0, invocation_counter.invocation_count_for_status(false));
  // Visible bounds and occluding bounds are now different.
  EXPECT_EQ(keyboard_window()->bounds(), notified_visible_bounds());
  EXPECT_EQ(gfx::Rect(), notified_occluding_bounds());
  EXPECT_TRUE(notified_is_visible());

  // callback should do nothing when container mode is set to the current active
  // container type. An unnecessary call gets registered synchronously as a
  // failure status to the callback.
  controller().SetContainerType(ContainerType::kFloating, base::nullopt,
                                invocation_counter.GetInvocationCallback());
  EXPECT_EQ(1, invocation_counter.invocation_count_for_status(true));
  EXPECT_EQ(1, invocation_counter.invocation_count_for_status(false));
}

TEST_F(KeyboardControllerAnimationTest, ChangeContainerModeWithBounds) {
  SetModeCallbackInvocationCounter invocation_counter;

  ui::Layer* layer = keyboard_window()->layer();
  ShowKeyboard();
  RunAnimationForLayer(layer);
  EXPECT_EQ(ContainerType::kFullWidth, controller().GetActiveContainerType());
  EXPECT_TRUE(keyboard_window()->IsVisible());

  // Changing the mode to another mode invokes hiding + showing.
  const gfx::Rect target_bounds(0, 0, 1200, 600);
  controller().SetContainerType(ContainerType::kFloating,
                                base::make_optional(target_bounds),
                                invocation_counter.GetInvocationCallback());
  // The container window shouldn't be resized until it's hidden even if the
  // target bounds is passed to |SetContainerType|.
  EXPECT_EQ(gfx::Rect(), notified_visible_bounds());
  EXPECT_EQ(0, invocation_counter.invocation_count_for_status(true));
  EXPECT_EQ(0, invocation_counter.invocation_count_for_status(false));
  RunAnimationForLayer(layer);
  // Hiding animation finished. The container window should be resized to the
  // target bounds.
  EXPECT_EQ(keyboard_window()->bounds().size(), target_bounds.size());
  // Then showing animation automatically start.
  layer = keyboard_window()->layer();
  RunAnimationForLayer(layer);
  EXPECT_EQ(1, invocation_counter.invocation_count_for_status(true));
  EXPECT_EQ(0, invocation_counter.invocation_count_for_status(false));
}

// Show keyboard during keyboard hide animation should abort the hide animation
// and the keyboard should animate in.
// Test for crbug.com/333284.
TEST_F(KeyboardControllerAnimationTest, ContainerShowWhileHide) {
  ui::Layer* layer = keyboard_window()->layer();
  ShowKeyboard();
  RunAnimationForLayer(layer);

  controller().HideKeyboardExplicitlyBySystem();
  // Before hide animation finishes, show keyboard again.
  ShowKeyboard();
  layer = keyboard_window()->layer();
  RunAnimationForLayer(layer);
  EXPECT_TRUE(keyboard_window()->IsVisible());
  EXPECT_EQ(1.0, layer->opacity());
}

TEST_F(KeyboardUIControllerTest, DisplayChangeShouldNotifyBoundsChange) {
  ui::DummyTextInputClient input_client(ui::TEXT_INPUT_TYPE_TEXT);

  SetFocus(&input_client);
  gfx::Rect new_bounds(0, 0, 1280, 800);
  ASSERT_NE(new_bounds, root_window()->bounds());
  EXPECT_EQ(1, visible_bounds_number_of_calls());
  EXPECT_EQ(1, occluding_bounds_number_of_calls());
  EXPECT_EQ(1, is_visible_number_of_calls());
  root_window()->SetBounds(new_bounds);
  EXPECT_EQ(2, visible_bounds_number_of_calls());
  EXPECT_EQ(2, occluding_bounds_number_of_calls());
  EXPECT_EQ(1, is_visible_number_of_calls());
  MockRotateScreen();
  EXPECT_EQ(3, visible_bounds_number_of_calls());
  EXPECT_EQ(3, occluding_bounds_number_of_calls());
  EXPECT_EQ(1, is_visible_number_of_calls());
}

TEST_F(KeyboardUIControllerTest, TextInputMode) {
  ui::DummyTextInputClient input_client(ui::TEXT_INPUT_TYPE_TEXT,
                                        ui::TEXT_INPUT_MODE_TEXT);
  ui::DummyTextInputClient no_input_client(ui::TEXT_INPUT_TYPE_TEXT,
                                           ui::TEXT_INPUT_MODE_NONE);

  base::RunLoop run_loop;
  aura::Window* keyboard_window = controller().GetKeyboardWindow();
  auto keyboard_container_observer =
      std::make_unique<KeyboardContainerObserver>(keyboard_window, &run_loop);

  SetFocus(&input_client);

  EXPECT_TRUE(keyboard_window->IsVisible());

  SetFocus(&no_input_client);
  // Keyboard should not immediately hide itself. It is delayed to avoid layout
  // flicker when the focus of input field quickly change.
  EXPECT_TRUE(keyboard_window->IsVisible());
  EXPECT_TRUE(WillHideKeyboard());
  // Wait for hide keyboard to finish.

  RunLoop(&run_loop);
  EXPECT_FALSE(keyboard_window->IsVisible());

  SetFocus(&input_client);
  EXPECT_TRUE(keyboard_window->IsVisible());
}

// Checks that floating keyboard does not cause focused window to move upwards.
// Refer to crbug.com/838731.
TEST_F(KeyboardControllerAnimationTest, FloatingKeyboardEnsureCaretInWorkArea) {
  // Mock TextInputClient to intercept calls to EnsureCaretNotInRect.
  struct MockTextInputClient : public ui::DummyTextInputClient {
    MockTextInputClient() : DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT) {}
    MOCK_METHOD(void, EnsureCaretNotInRect, (const gfx::Rect&), (override));
  };

  // Floating keyboard should call EnsureCaretNotInRect with the empty rect.
  MockTextInputClient mock_input_client;
  EXPECT_CALL(mock_input_client, EnsureCaretNotInRect(gfx::Rect())).Times(1);

  controller().SetContainerType(ContainerType::kFloating, base::nullopt,
                                base::DoNothing());
  ASSERT_EQ(ContainerType::kFloating, controller().GetActiveContainerType());

  // Ensure keyboard ui is populated
  ui::Layer* layer = keyboard_window()->layer();
  SetFocus(&mock_input_client);
  RunAnimationForLayer(layer);

  EXPECT_TRUE(keyboard_window()->IsVisible());

  // Unfocus from the MockTextInputClient before destroying it.
  controller().GetInputMethodForTest()->DetachTextInputClient(
      &mock_input_client);
}

// Checks DisableKeyboard() doesn't clear the observer list.
TEST_F(KeyboardUIControllerTest, DontClearObserverList) {
  ShowKeyboard();
  aura::Window* keyboard_window = controller().GetKeyboardWindow();

  ShowKeyboard();
  EXPECT_TRUE(keyboard_window->IsVisible());
  EXPECT_FALSE(IsKeyboardDisabled());

  keyboard::SetTouchKeyboardEnabled(false);
  EXPECT_TRUE(IsKeyboardDisabled());

  keyboard::SetTouchKeyboardEnabled(true);
  ClearKeyboardDisabled();
  EXPECT_FALSE(IsKeyboardDisabled());

  keyboard::SetTouchKeyboardEnabled(false);
  EXPECT_TRUE(IsKeyboardDisabled());
}

// Checks the area set in SetAreaToRemainOnScreen fit within the
// bounds of the keyboard window.
TEST_F(KeyboardUIControllerTest,
       AreaToRemainOnScreenShouldBeContainedInWindow) {
  ShowKeyboard();
  aura::Window* keyboard_window = controller().GetKeyboardWindow();
  keyboard_window->SetBounds(gfx::Rect(10, 10, 400, 600));

  EXPECT_TRUE(controller().SetAreaToRemainOnScreen(gfx::Rect(0, 0, 100, 200)));
  EXPECT_FALSE(controller().SetAreaToRemainOnScreen(gfx::Rect(0, 0, 450, 650)));
  EXPECT_FALSE(
      controller().SetAreaToRemainOnScreen(gfx::Rect(50, 50, 400, 600)));
}

class MockKeyboardControllerObserver : public ash::KeyboardControllerObserver {
 public:
  MockKeyboardControllerObserver() = default;
  ~MockKeyboardControllerObserver() override = default;

  // KeyboardControllerObserver:
  MOCK_METHOD(void, OnKeyboardEnabledChanged, (bool is_enabled), (override));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockKeyboardControllerObserver);
};

TEST_F(KeyboardUIControllerTest, OnKeyboardEnabledChangedToEnabled) {
  // Start with the keyboard disabled.
  keyboard::SetTouchKeyboardEnabled(false);

  MockKeyboardControllerObserver mock_observer;
  controller().AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnKeyboardEnabledChanged(true))
      .WillOnce(testing::InvokeWithoutArgs([]() {
        auto* controller = keyboard::KeyboardUIController::Get();
        ASSERT_TRUE(controller);
        EXPECT_TRUE(controller->IsEnabled());
        EXPECT_TRUE(controller->GetKeyboardWindow());
        EXPECT_TRUE(controller->GetRootWindow());
      }));

  keyboard::SetTouchKeyboardEnabled(true);

  controller().RemoveObserver(&mock_observer);
}

TEST_F(KeyboardUIControllerTest, OnKeyboardEnabledChangedToDisabled) {
  MockKeyboardControllerObserver mock_observer;
  controller().AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnKeyboardEnabledChanged(false))
      .WillOnce(testing::InvokeWithoutArgs([]() {
        auto* controller = keyboard::KeyboardUIController::Get();
        ASSERT_TRUE(controller);
        EXPECT_FALSE(controller->IsEnabled());
        EXPECT_FALSE(controller->GetKeyboardWindow());
        EXPECT_FALSE(controller->GetRootWindow());
      }));

  keyboard::SetTouchKeyboardEnabled(false);

  controller().RemoveObserver(&mock_observer);
}

}  // namespace keyboard

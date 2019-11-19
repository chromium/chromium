// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lock_screen_action/lock_screen_action_background_controller_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/lock_screen_action/lock_screen_action_background_controller_impl_test_api.h"
#include "ash/lock_screen_action/lock_screen_action_background_observer.h"
#include "ash/lock_screen_action/lock_screen_action_background_state.h"
#include "ash/lock_screen_action/lock_screen_action_background_view.h"
#include "ash/lock_screen_action/lock_screen_action_background_view_test_api.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

aura::Window* GetContainer(ShellWindowId container_id) {
  return Shell::GetPrimaryRootWindowController()->GetContainer(container_id);
}

class TestActionBackgroundObserver : public LockScreenActionBackgroundObserver {
 public:
  TestActionBackgroundObserver() = default;
  ~TestActionBackgroundObserver() override = default;

  // LockScreenActionBackgroundObserver:
  void OnLockScreenActionBackgroundStateChanged(
      LockScreenActionBackgroundState state) override {
    state_changes_.push_back(state);
  }

  const std::vector<LockScreenActionBackgroundState> state_changes() const {
    return state_changes_;
  }

  void ClearStateChanges() { state_changes_.clear(); }

 private:
  std::vector<LockScreenActionBackgroundState> state_changes_;

  DISALLOW_COPY_AND_ASSIGN(TestActionBackgroundObserver);
};

}  // namespace

class LockScreenActionBackgroundControllerImplTest : public AshTestBase {
 public:
  LockScreenActionBackgroundControllerImplTest() = default;
  ~LockScreenActionBackgroundControllerImplTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    test_animation_duration_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(
            ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
    controller_.AddObserver(&observer_);
  }

  void TearDown() override {
    controller_.RemoveObserver(&observer_);
    test_animation_duration_.reset();
    AshTestBase::TearDown();
  }

  void CompleteBackgroundAnimations() {
    LockScreenActionBackgroundView* contents_view =
        LockScreenActionBackgroundControllerImplTestApi(controller())
            .GetContentsView();
    views::View* background =
        LockScreenActionBackgroundViewTestApi(contents_view).GetBackground();
    // The ink drop's layer is a sibling of the background's layer, so call this
    // from the background's layer's parent.
    background->layer()->parent()->CompleteAllAnimations();
  }

  views::Widget* GetBackgroundWidget() {
    return LockScreenActionBackgroundControllerImplTestApi(controller())
        .GetWidget();
  }

  aura::Window* GetBackgroundWindow() {
    views::Widget* widget = GetBackgroundWidget();
    if (!widget)
      return nullptr;
    return widget->GetNativeWindow();
  }

  bool BackgroundWindowShown() {
    aura::Window* window = GetBackgroundWindow();
    if (!window)
      return false;
    return window->IsVisible();
  }

  void ExpectObservedStatesMatch(
      const std::vector<LockScreenActionBackgroundState>& expectation,
      const std::string& error_message) {
    EXPECT_EQ(expectation, observer_.state_changes()) << error_message;
    observer_.ClearStateChanges();
  }

  // Initializes the controller instance to the desired state.
  bool InitializeControllerToState(LockScreenActionBackgroundState state) {
    controller_.SetParentWindow(
        GetContainer(kShellWindowId_LockScreenContainer));
    if (state == LockScreenActionBackgroundState::kHidden)
      return controller_.state() == state;

    controller_.ShowBackground();
    observer_.ClearStateChanges();

    if (state == LockScreenActionBackgroundState::kShowing)
      return controller_.state() == state;

    CompleteBackgroundAnimations();
    observer_.ClearStateChanges();

    if (state == LockScreenActionBackgroundState::kShown)
      return controller_.state() == state;

    controller_.HideBackground();
    observer_.ClearStateChanges();

    return controller_.state() == state;
  }

  LockScreenActionBackgroundControllerImpl* controller() {
    return &controller_;
  }

 private:
  LockScreenActionBackgroundControllerImpl controller_;
  TestActionBackgroundObserver observer_;

  std::unique_ptr<ui::ScopedAnimationDurationScaleMode>
      test_animation_duration_;

  DISALLOW_COPY_AND_ASSIGN(LockScreenActionBackgroundControllerImplTest);
};

TEST_F(LockScreenActionBackgroundControllerImplTest, NormalFlow) {
  // Show/Hide before parent window is set should fail.
  EXPECT_FALSE(controller()->ShowBackground());
  EXPECT_FALSE(controller()->HideBackgroundImmediately());
  EXPECT_FALSE(controller()->HideBackground());
  EXPECT_EQ(LockScreenActionBackgroundState::kHidden, controller()->state());

  controller()->SetParentWindow(
      GetContainer(kShellWindowId_LockScreenContainer));

  // With parent window set, show request should make the window visible and
  // move the controller to showing state.
  ASSERT_TRUE(controller()->ShowBackground());

  EXPECT_EQ(LockScreenActionBackgroundState::kShowing, controller()->state());
  ExpectObservedStatesMatch({LockScreenActionBackgroundState::kShowing},
                            "Show requested");
  EXPECT_TRUE(BackgroundWindowShown());
  EXPECT_FALSE(GetBackgroundWindow()->HasFocus());

  // Repeated request to show a background window should fail.
  EXPECT_FALSE(controller()->ShowBackground());
  EXPECT_EQ(LockScreenActionBackgroundState::kShowing, controller()->state());

  // When the background window animations are complete, the controller should
  // report kShown state.
  CompleteBackgroundAnimations();

  EXPECT_EQ(LockScreenActionBackgroundState::kShown, controller()->state());
  ExpectObservedStatesMatch({LockScreenActionBackgroundState::kShown},
                            "Show animation complete");
  EXPECT_TRUE(BackgroundWindowShown());
  EXPECT_FALSE(GetBackgroundWindow()->HasFocus());

  // Request to show a background window in shown state should fail.
  EXPECT_FALSE(controller()->ShowBackground());
  EXPECT_EQ(LockScreenActionBackgroundState::kShown, controller()->state());

  // Request to hide the window should start hiding the background window
  // (the window should still be visible at this point).
  ASSERT_TRUE(controller()->HideBackground());

  EXPECT_EQ(LockScreenActionBackgroundState::kHiding, controller()->state());
  ExpectObservedStatesMatch({LockScreenActionBackgroundState::kHiding},
                            "Hide requested");
  EXPECT_TRUE(BackgroundWindowShown());
  EXPECT_FALSE(GetBackgroundWindow()->HasFocus());

  // Repeated reqiest to hide the background window should fail.
  EXPECT_FALSE(controller()->HideBackground());
  EXPECT_EQ(LockScreenActionBackgroundState::kHiding, controller()->state());

  // The window should be hidden when the background window animations are
  // completed.
  CompleteBackgroundAnimations();

  EXPECT_EQ(LockScreenActionBackgroundState::kHidden, controller()->state());
  ExpectObservedStatesMatch({LockScreenActionBackgroundState::kHidden},
                            "Hide animation complete");
  EXPECT_FALSE(BackgroundWindowShown());
  EXPECT_EQ(LockScreenActionBackgroundState::kHidden, controller()->state());

  // Request to hide the background window should fail if the window is already
  // hidden.
  EXPECT_FALSE(controller()->HideBackground());
  EXPECT_EQ(LockScreenActionBackgroundState::kHidden, controller()->state());
}

TEST_F(LockScreenActionBackgroundControllerImplTest, HideWhileShowing) {
  ASSERT_TRUE(
      InitializeControllerToState(LockScreenActionBackgroundState::kShowing));

  // Request to hide background while show is in progress should initiate the
  // background window hide.
  ASSERT_TRUE(controller()->HideBackground());
  EXPECT_EQ(LockScreenActionBackgroundState::kHiding, controller()->state());
  ExpectObservedStatesMatch({LockScreenActionBackgroundState::kHiding},
                            "Hide requested");
  EXPECT_TRUE(BackgroundWindowShown());

  // Upon animation completion, the background window should be hidden.
  CompleteBackgroundAnimations();
  EXPECT_EQ(LockScreenActionBackgroundState::kHidden, controller()->state());
  ExpectObservedStatesMatch({LockScreenActionBackgroundState::kHidden},
                            "Hide animation complete");
  EXPECT_FALSE(BackgroundWindowShown());
  EXPECT_EQ(LockScreenActionBackgroundState::kHidden, controller()->state());
}

TEST_F(LockScreenActionBackgroundControllerImplTest,
       HideImmediatelyWhileShowing) {
  ASSERT_TRUE(
      InitializeControllerToState(LockScreenActionBackgroundState::kShowing));

  // Request to hide background while show is in progress should hide the
  // background window.
  ASSERT_TRUE(controller()->HideBackgroundImmediately());
  EXPECT_EQ(LockScreenActionBackgroundState::kHidden, controller()->state());
  ExpectObservedStatesMatch({LockScreenActionBackgroundState::kHidden},
                            "Hide immediately requested");
  EXPECT_FALSE(BackgroundWindowShown());

  // Run any pending animations, to confirm that the controller state does not
  // change.
  CompleteBackgroundAnimations();
  EXPECT_EQ(LockScreenActionBackgroundState::kHidden, controller()->state());
  ExpectObservedStatesMatch(std::vector<LockScreenActionBackgroundState>(),
                            "Running animation on hidden window");
  EXPECT_FALSE(BackgroundWindowShown());
}

TEST_F(LockScreenActionBackgroundControllerImplTest, HideImmediatelyWhenShown) {
  ASSERT_TRUE(
      InitializeControllerToState(LockScreenActionBackgroundState::kShown));

  // Request to hide background immediately - the window should get hidden.
  ASSERT_TRUE(controller()->HideBackgroundImmediately());
  EXPECT_EQ(LockScreenActionBackgroundState::kHidden, controller()->state());
  ExpectObservedStatesMatch({LockScreenActionBackgroundState::kHidden},
                            "Hide immediately requested");
  EXPECT_FALSE(BackgroundWindowShown());

  // Run any pending animations, to confirm that the controller state does not
  // change.
  CompleteBackgroundAnimations();
  EXPECT_EQ(LockScreenActionBackgroundState::kHidden, controller()->state());
  ExpectObservedStatesMatch(std::vector<LockScreenActionBackgroundState>(),
                            "Running animation on hidden window");
  EXPECT_FALSE(BackgroundWindowShown());
}

TEST_F(LockScreenActionBackgroundControllerImplTest,
       HideImmediatelyWhileHiding) {
  ASSERT_TRUE(
      InitializeControllerToState(LockScreenActionBackgroundState::kHiding));

  // Request to hide background immediately while hide is in progress should
  // hide the background window.
  ASSERT_TRUE(controller()->HideBackgroundImmediately());
  EXPECT_EQ(LockScreenActionBackgroundState::kHidden, controller()->state());
  ExpectObservedStatesMatch({LockScreenActionBackgroundState::kHidden},
                            "Hide immediately requested");
  EXPECT_FALSE(BackgroundWindowShown());

  // Run any pending animations, to confirm that the controller state does not
  // change.
  CompleteBackgroundAnimations();
  EXPECT_EQ(LockScreenActionBackgroundState::kHidden, controller()->state());
  ExpectObservedStatesMatch(std::vector<LockScreenActionBackgroundState>(),
                            "Running animation on hidden window");
  EXPECT_FALSE(BackgroundWindowShown());
}

TEST_F(LockScreenActionBackgroundControllerImplTest, ShowWhileHiding) {
  ASSERT_TRUE(
      InitializeControllerToState(LockScreenActionBackgroundState::kHiding));

  // Request to show the background window while hide ainmation is in progress
  // should be respected.
  ASSERT_TRUE(controller()->ShowBackground());
  EXPECT_EQ(LockScreenActionBackgroundState::kShowing, controller()->state());
  ExpectObservedStatesMatch({LockScreenActionBackgroundState::kShowing},
                            "Show requested");
  EXPECT_TRUE(BackgroundWindowShown());

  // When the animations are completed, the controller should be in kShown
  // state.
  CompleteBackgroundAnimations();
  EXPECT_EQ(LockScreenActionBackgroundState::kShown, controller()->state());
  ExpectObservedStatesMatch({LockScreenActionBackgroundState::kShown},
                            "Complete show animation");
  EXPECT_TRUE(BackgroundWindowShown());
}

TEST_F(LockScreenActionBackgroundControllerImplTest,
       BackgroundWindowClosureWhenShown) {
  ASSERT_TRUE(
      InitializeControllerToState(LockScreenActionBackgroundState::kShown));

  // Close the background window while show is in progress - the controller
  // should move to kHidden state.
  ASSERT_TRUE(BackgroundWindowShown());
  GetBackgroundWidget()->Close();
  // Run the loop so the close request propagates.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(LockScreenActionBackgroundState::kHidden, controller()->state());
  EXPECT_FALSE(BackgroundWindowShown());

  // Background should be showable again, if requsted.
  ASSERT_TRUE(controller()->ShowBackground());
  EXPECT_EQ(LockScreenActionBackgroundState::kShowing, controller()->state());
  EXPECT_TRUE(BackgroundWindowShown());

  CompleteBackgroundAnimations();
  EXPECT_EQ(LockScreenActionBackgroundState::kShown, controller()->state());
  EXPECT_TRUE(BackgroundWindowShown());
}

TEST_F(LockScreenActionBackgroundControllerImplTest,
       BackgroundWindowClosureWhenHidden) {
  ASSERT_TRUE(
      InitializeControllerToState(LockScreenActionBackgroundState::kHiding));
  CompleteBackgroundAnimations();
  EXPECT_EQ(LockScreenActionBackgroundState::kHidden, controller()->state());

  // Close the background window while show is in progress - the controller
  // should move to kHidden state.
  ASSERT_TRUE(GetBackgroundWidget());
  GetBackgroundWidget()->Close();
  // Run the loop so the close request propagates.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(LockScreenActionBackgroundState::kHidden, controller()->state());
  EXPECT_FALSE(BackgroundWindowShown());

  // Background should be showable again, if requsted.
  ASSERT_TRUE(controller()->ShowBackground());
  EXPECT_EQ(LockScreenActionBackgroundState::kShowing, controller()->state());
  EXPECT_TRUE(BackgroundWindowShown());

  CompleteBackgroundAnimations();
  EXPECT_EQ(LockScreenActionBackgroundState::kShown, controller()->state());
  EXPECT_TRUE(BackgroundWindowShown());
}

TEST_F(LockScreenActionBackgroundControllerImplTest,
       BackgroundWindowClosureWhileShowing) {
  ASSERT_TRUE(
      InitializeControllerToState(LockScreenActionBackgroundState::kShowing));

  // Close the background window while show is in progress - the controller
  // should move to kHidden state.
  ASSERT_TRUE(BackgroundWindowShown());
  GetBackgroundWidget()->Close();
  // Run the loop so the close request propagates.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(LockScreenActionBackgroundState::kHidden, controller()->state());
  EXPECT_FALSE(BackgroundWindowShown());
}

TEST_F(LockScreenActionBackgroundControllerImplTest,
       BackgroundWindowClosureWhileHiding) {
  ASSERT_TRUE(
      InitializeControllerToState(LockScreenActionBackgroundState::kHiding));

  // Close the background window while hiding is in progress - the controller
  // should move to kHidden state.
  ASSERT_TRUE(BackgroundWindowShown());
  GetBackgroundWidget()->Close();
  // Run the loop so the close request propagates.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(LockScreenActionBackgroundState::kHidden, controller()->state());
  EXPECT_FALSE(BackgroundWindowShown());
}

}  // namespace ash

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/session_state_animator_impl.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/session_state_animator.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "ui/aura/client/aura_constants.h"

namespace ash {
namespace {

bool ParentHasWindowWithId(const aura::Window* window, int id) {
  return window->parent()->GetId() == id;
}

bool ContainersHaveWindowWithId(const aura::Window::Windows windows, int id) {
  for (const aura::Window* window : windows) {
    if (window->GetId() == id)
      return true;
  }
  return false;
}

}  // namespace

using SessionStateAnimatiorImplContainersTest = AshTestBase;

TEST_F(SessionStateAnimatiorImplContainersTest, ContainersHaveIdTest) {
  aura::Window::Windows containers;

  // Test ROOT_CONTAINER mask.
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  SessionStateAnimatorImpl::GetContainers(SessionStateAnimator::ROOT_CONTAINER,
                                          &containers);
  EXPECT_EQ(root_window, containers[0]);

  containers.clear();

  SessionStateAnimatorImpl::GetContainers(SessionStateAnimator::WALLPAPER,
                                          &containers);
  EXPECT_TRUE(ContainersHaveWindowWithId(containers,
                                         kShellWindowId_WallpaperContainer));

  containers.clear();

  // Check for shelf.
  SessionStateAnimatorImpl::GetContainers(SessionStateAnimator::SHELF,
                                          &containers);
  EXPECT_TRUE(
      ContainersHaveWindowWithId(containers, kShellWindowId_ShelfContainer));

  containers.clear();

  SessionStateAnimatorImpl::GetContainers(
      SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS, &containers);
  EXPECT_TRUE(ParentHasWindowWithId(
      containers[0], kShellWindowId_NonLockScreenContainersContainer));
  // Verify the containers inside `NON_LOCK_SCREEN_CONTAINERS` be animated.
  auto iter = std::find(containers.begin(), containers.end(),
                        desks_util::GetActiveDeskContainerForRoot(root_window));
  EXPECT_TRUE(iter != containers.end());
  for (const int id :
       SessionStateAnimatorImpl::ContainersToAnimateInNonLockScreenContainer) {
    iter = std::find(containers.begin(), containers.end(),
                     Shell::GetContainer(root_window, id));
    EXPECT_TRUE(iter != containers.end());
  }

  containers.clear();

  // Check for lock screen containers.
  SessionStateAnimatorImpl::GetContainers(
      SessionStateAnimator::LOCK_SCREEN_WALLPAPER, &containers);
  EXPECT_TRUE(ContainersHaveWindowWithId(
      containers, kShellWindowId_LockScreenWallpaperContainer));

  containers.clear();

  // Check for the lock screen containers container.
  SessionStateAnimatorImpl::GetContainers(
      SessionStateAnimator::LOCK_SCREEN_RELATED_CONTAINERS, &containers);
  EXPECT_TRUE(ContainersHaveWindowWithId(
      containers, kShellWindowId_LockScreenRelatedContainersContainer));

  // Empty mask should clear the container.
  SessionStateAnimatorImpl::GetContainers(0, &containers);
  EXPECT_TRUE(containers.empty());
}

// Test that SessionStateAnimatorImpl invokes the callback only once on
// multi-display env, where it needs to run multiple animations on multiple
// containers. See http://crbug.com/712422 for details.
TEST_F(SessionStateAnimatiorImplContainersTest,
       AnimationCallbackOnMultiDisplay) {
  UpdateDisplay("300x200,500x400");

  int callback_count = 0;
  SessionStateAnimatorImpl animator;
  animator.StartAnimationWithCallback(
      SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
      SessionStateAnimator::ANIMATION_LIFT,
      SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE,
      base::BindOnce([](int* count) { ++(*count); }, &callback_count));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback_count);
}

// Tests that AnimationSequence is not released prematurely because
// LayerCopyAnimator aborts animations due to display size change.
TEST_F(SessionStateAnimatiorImplContainersTest,
       AnimationSequenceAndLayerCopyAnimator) {
  UpdateDisplay("300x200,500x400");
  base::RunLoop().RunUntilIdle();

  // Create windows in containers of all displays so that the containers will
  // be animated.
  auto window_1 = CreateTestWindow(gfx::Rect(0, 0, 30, 20));
  auto window_2 = CreateTestWindow(gfx::Rect(600, 0, 30, 20));

  SessionStateAnimatorImpl animator;

  // Creates LayerCopyAnimator on containers.
  animator.StartAnimation(SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
                          SessionStateAnimator::ANIMATION_COPY_LAYER,
                          SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);

  // Simulate display changes that cause LayerCopyAnimators of first part of
  // containers list to fail.
  UpdateDisplay("600x500,500x400");

  base::RunLoop animation_wait_loop;
  auto animation_ended = [&](bool) { animation_wait_loop.Quit(); };

  // Start a ANIMATION_DROP sequence.
  SessionStateAnimator::AnimationSequence* animation_sequence =
      animator.BeginAnimationSequence(
          base::BindLambdaForTesting(animation_ended));
  animation_sequence->StartAnimation(
      SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      SessionStateAnimator::ANIMATION_DROP,
      SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  animation_sequence->EndSequence();

  // Wait for `animation_ended` to be called.
  animation_wait_loop.Run();

  // No crash or use-after-free should happen.
}

}  // namespace ash

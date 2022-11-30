// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/session_state_animator_impl.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/session_state_animator.h"
#include "base/bind.h"
#include "base/run_loop.h"
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

}  // namespace ash

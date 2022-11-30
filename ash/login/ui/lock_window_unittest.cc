// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_big_user_view.h"
#include "ash/login/ui/login_keyboard_test_base.h"
#include "ash/login/ui/login_test_utils.h"
#include "ui/wm/core/capture_controller.h"

namespace ash {

using LockWindowVirtualKeyboardTest = LoginKeyboardTestBase;

TEST_F(LockWindowVirtualKeyboardTest, VirtualKeyboardDoesNotCoverAuthView) {
  ASSERT_NO_FATAL_FAILURE(ShowLockScreen());
  LockContentsView* lock_contents =
      LockScreen::TestApi(LockScreen::Get()).contents_view();
  ASSERT_NE(nullptr, lock_contents);

  SetUserCount(1);

  LoginBigUserView* auth_view =
      MakeLockContentsViewTestApi(lock_contents).primary_big_view();
  ASSERT_NE(nullptr, auth_view);

  ASSERT_NO_FATAL_FAILURE(ShowKeyboard());
  EXPECT_FALSE(
      auth_view->GetBoundsInScreen().Intersects(GetKeyboardBoundsInScreen()));
}

TEST_F(LockWindowVirtualKeyboardTest, ReleaseCapture) {
  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  ::wm::CaptureController::Get()->SetCapture(window0.get());
  ASSERT_EQ(window0.get(), ::wm::CaptureController::Get()->GetCaptureWindow());

  ASSERT_NO_FATAL_FAILURE(ShowLockScreen());
  EXPECT_FALSE(::wm::CaptureController::Get()->GetCaptureWindow());
}

}  // namespace ash

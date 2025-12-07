// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"

#include "base/run_loop.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/webui_login_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"

namespace ash {

OobeScreenWaiter::OobeScreenWaiter(OobeScreenId target_screen)
    : target_screen_(target_screen) {}

OobeScreenWaiter::~OobeScreenWaiter() = default;

void OobeScreenWaiter::Wait() {
  DCHECK_EQ(State::IDLE, state_);

  if (CheckIfDone())
    return;

  DCHECK(!run_loop_);

  state_ = State::WAITING_FOR_SCREEN;

  test::WaitForOobeJSReady();

  if (CheckIfDone())
    return;

  oobe_ui_observation_.Observe(GetOobeUI());
  if (check_native_window_visible_) {
    aura::Window* native_window =
        LoginDisplayHost::default_host()->GetNativeWindow();
    DCHECK(native_window);
    native_window_observation_.Observe(native_window);
  }

  LOG(INFO) << "Actually waiting for screen " << target_screen_.name;

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();

  ASSERT_EQ(State::DONE, state_)
      << " Timed out while waiting for " << target_screen_.name;

  oobe_ui_observation_.Reset();
  if (check_native_window_visible_)
    native_window_observation_.Reset();

  if (assert_last_screen_)
    EXPECT_EQ(target_screen_, GetOobeUI()->current_screen());
}

void OobeScreenWaiter::OnCurrentScreenChanged(OobeScreenId current_screen,
                                              OobeScreenId new_screen) {
  DCHECK_NE(state_, State::IDLE);

  if (state_ != State::WAITING_FOR_SCREEN) {
    if (assert_last_screen_ && new_screen != target_screen_) {
      ADD_FAILURE() << "Screen changed from the target screen "
                    << current_screen.name << " -> " << new_screen.name;
      EndWait();
    }
    return;
  }

  if (assert_next_screen_ && new_screen != target_screen_) {
    ADD_FAILURE() << "Untarget screen change to " << new_screen.name
                  << " while waiting for " << target_screen_.name;
    EndWait();
    return;
  }

  if (check_native_window_visible_ && !IsNativeWindowVisible()) {
    return;
  }

  if (IsTargetScreenReached())
    EndWait();
}

void OobeScreenWaiter::OnWindowVisibilityChanged(aura::Window* window,
                                                 bool visible) {
  DCHECK(check_native_window_visible_);

  if (IsNativeWindowVisible() && IsTargetScreenReached())
    EndWait();
}

bool OobeScreenWaiter::IsTargetScreenReached() {
  return LoginDisplayHost::default_host()->GetOobeUI() &&
         GetOobeUI()->current_screen() == target_screen_;
}

bool OobeScreenWaiter::IsNativeWindowVisible() {
  aura::Window* native_window =
      LoginDisplayHost::default_host()->GetNativeWindow();
  return native_window && native_window->IsVisible();
}

void OobeScreenWaiter::OnDestroyingOobeUI() {
  oobe_ui_observation_.Reset();

  EXPECT_EQ(State::DONE, state_);

  EndWait();
}

OobeUI* OobeScreenWaiter::GetOobeUI() {
  OobeUI* oobe_ui = LoginDisplayHost::default_host()->GetOobeUI();
  CHECK(oobe_ui);
  return oobe_ui;
}

bool OobeScreenWaiter::CheckIfDone() {
  if ((!check_native_window_visible_ || IsNativeWindowVisible()) &&
      IsTargetScreenReached()) {
    state_ = State::DONE;
    return true;
  }
  return false;
}

void OobeScreenWaiter::EndWait() {
  if (state_ == State::DONE)
    return;

  state_ = State::DONE;
  run_loop_->Quit();
}

}  // namespace ash

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"

#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"

namespace chromeos {

OobeScreenWaiter::OobeScreenWaiter(OobeScreenId target_screen)
    : target_screen_(target_screen) {}

OobeScreenWaiter::~OobeScreenWaiter() = default;

void OobeScreenWaiter::Wait() {
  DCHECK_EQ(State::IDLE, state_);

  if ((!check_native_window_visible_ || IsNativeWindowVisible()) &&
      IsTargetScreenReached()) {
    state_ = State::DONE;
    return;
  }
  DCHECK(!run_loop_);

  oobe_ui_observer_.Add(GetOobeUI());
  if (check_native_window_visible_) {
    aura::Window* native_window =
        LoginDisplayHost::default_host()->GetNativeWindow();
    DCHECK(native_window);
    native_window_observer_.Add(native_window);
  }

  state_ = State::WAITING_FOR_SCREEN;

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();

  DCHECK_EQ(State::DONE, state_);

  oobe_ui_observer_.RemoveAll();
  if (check_native_window_visible_)
    native_window_observer_.RemoveAll();

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
  DCHECK_NE(state_, State::IDLE);
  DCHECK(check_native_window_visible_);

  if (IsNativeWindowVisible() && IsTargetScreenReached())
    EndWait();
}

bool OobeScreenWaiter::IsTargetScreenReached() {
  return GetOobeUI()->current_screen() == target_screen_;
}

bool OobeScreenWaiter::IsNativeWindowVisible() {
  aura::Window* native_window =
      LoginDisplayHost::default_host()->GetNativeWindow();
  return native_window && native_window->IsVisible();
}

void OobeScreenWaiter::OnDestroyingOobeUI() {
  oobe_ui_observer_.RemoveAll();

  EXPECT_EQ(State::DONE, state_);

  EndWait();
}

OobeUI* OobeScreenWaiter::GetOobeUI() {
  OobeUI* oobe_ui = LoginDisplayHost::default_host()->GetOobeUI();
  CHECK(oobe_ui);
  return oobe_ui;
}

void OobeScreenWaiter::EndWait() {
  if (state_ == State::DONE)
    return;

  state_ = State::DONE;
  run_loop_->Quit();
}

}  // namespace chromeos

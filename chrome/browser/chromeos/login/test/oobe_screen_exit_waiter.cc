// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/oobe_screen_exit_waiter.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"

namespace chromeos {

OobeScreenExitWaiter::OobeScreenExitWaiter(OobeScreenId target_screen)
    : target_screen_(target_screen) {}

OobeScreenExitWaiter::~OobeScreenExitWaiter() = default;

void OobeScreenExitWaiter::Wait() {
  DCHECK_EQ(State::IDLE, state_);

  OobeUI* oobe_ui = GetOobeUI();
  if (!oobe_ui || oobe_ui->current_screen() != target_screen_) {
    state_ = State::DONE;
    return;
  }
  DCHECK(!run_loop_);

  oobe_ui_observer_.Add(GetOobeUI());

  state_ = State::WAITING_FOR_SCREEN_EXIT;

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();

  DCHECK_EQ(State::DONE, state_);

  oobe_ui_observer_.RemoveAll();
}

void OobeScreenExitWaiter::OnCurrentScreenChanged(OobeScreenId current_screen,
                                                  OobeScreenId new_screen) {
  DCHECK_NE(state_, State::IDLE);
  if (new_screen != target_screen_)
    EndWait();
}

void OobeScreenExitWaiter::OnDestroyingOobeUI() {
  oobe_ui_observer_.RemoveAll();
  EndWait();
}

OobeUI* OobeScreenExitWaiter::GetOobeUI() {
  if (!LoginDisplayHost::default_host())
    return nullptr;
  return LoginDisplayHost::default_host()->GetOobeUI();
}

void OobeScreenExitWaiter::EndWait() {
  if (state_ == State::DONE)
    return;

  state_ = State::DONE;
  run_loop_->Quit();
}

}  // namespace chromeos

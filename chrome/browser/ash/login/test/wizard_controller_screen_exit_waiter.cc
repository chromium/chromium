// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/wizard_controller_screen_exit_waiter.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

WizardControllerExitWaiter::WizardControllerExitWaiter(OobeScreenId screen_id)
    : target_screen_id_(screen_id) {}

WizardControllerExitWaiter::~WizardControllerExitWaiter() = default;

void WizardControllerExitWaiter::Wait() {
  ASSERT_EQ(State::IDLE, state_);

  WizardController* wizard_controller = WizardController::default_controller();
  if (!wizard_controller ||
      wizard_controller->current_screen()->screen_id() != target_screen_id_) {
    state_ = State::DONE;
    return;
  }
  ASSERT_FALSE(run_loop_);

  screen_observation_.Observe(wizard_controller);

  state_ = State::WAITING_FOR_SCREEN_EXIT;

  LOG(INFO) << "Actually waiting for exiting screen " << target_screen_id_;

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();

  ASSERT_EQ(State::DONE, state_);

  screen_observation_.Reset();
}

void WizardControllerExitWaiter::OnCurrentScreenChanged(
    BaseScreen* new_screen) {
  ASSERT_NE(state_, State::IDLE);
  if (new_screen->screen_id() != target_screen_id_)
    EndWait();
}

void WizardControllerExitWaiter::OnShutdown() {
  screen_observation_.Reset();
  EndWait();
}

void WizardControllerExitWaiter::EndWait() {
  if (state_ == State::DONE)
    return;

  state_ = State::DONE;
  run_loop_->Quit();
}

}  // namespace ash

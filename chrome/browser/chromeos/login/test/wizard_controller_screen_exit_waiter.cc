// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/wizard_controller_screen_exit_waiter.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

WizardControllerExitWaiter::WizardControllerExitWaiter(OobeScreenId screen_id)
    : WizardControllerExitWaiter(
          WizardController::default_controller()->GetScreen(screen_id)) {}

WizardControllerExitWaiter::WizardControllerExitWaiter(
    BaseScreen* target_screen)
    : target_screen_(target_screen) {}

WizardControllerExitWaiter::~WizardControllerExitWaiter() = default;

void WizardControllerExitWaiter::Wait() {
  ASSERT_EQ(State::IDLE, state_);

  WizardController* wizard_controller = WizardController::default_controller();
  if (!wizard_controller ||
      wizard_controller->current_screen() != target_screen_) {
    state_ = State::DONE;
    return;
  }
  ASSERT_FALSE(run_loop_);

  screen_observer_.Add(wizard_controller);

  state_ = State::WAITING_FOR_SCREEN_EXIT;

  LOG(INFO) << "Actually waiting for exiting screen "
            << target_screen_->screen_id();

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();

  ASSERT_EQ(State::DONE, state_);

  screen_observer_.RemoveAll();
}

void WizardControllerExitWaiter::OnCurrentScreenChanged(
    BaseScreen* new_screen) {
  ASSERT_NE(state_, State::IDLE);
  if (new_screen != target_screen_)
    EndWait();
}

void WizardControllerExitWaiter::OnShutdown() {
  screen_observer_.RemoveAll();
  EndWait();
}

void WizardControllerExitWaiter::EndWait() {
  if (state_ == State::DONE)
    return;

  state_ = State::DONE;
  run_loop_->Quit();
}

}  // namespace chromeos

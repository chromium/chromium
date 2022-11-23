// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_WIZARD_CONTROLLER_SCREEN_EXIT_WAITER_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_WIZARD_CONTROLLER_SCREEN_EXIT_WAITER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"

namespace base {
class RunLoop;
}

namespace ash {

// A waiter that blocks until the the current WizardController screen is
// different than the target screen, or the WizardController is destroyed.
class WizardControllerExitWaiter : public test::TestConditionWaiter,
                                   public WizardController::ScreenObserver {
 public:
  explicit WizardControllerExitWaiter(OobeScreenId screen_id);
  ~WizardControllerExitWaiter() override;

  // WizardController::ScreenObserver:
  void OnCurrentScreenChanged(BaseScreen* new_screen) override;
  void OnShutdown() override;

  // TestConditionWaiter;
  void Wait() override;

 private:
  enum class State { IDLE, WAITING_FOR_SCREEN_EXIT, DONE };

  void EndWait();

  const OobeScreenId target_screen_id_ = OOBE_SCREEN_UNKNOWN;

  State state_ = State::IDLE;

  base::ScopedObservation<WizardController, WizardController::ScreenObserver>
      screen_observation_{this};

  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_WIZARD_CONTROLLER_SCREEN_EXIT_WAITER_H_

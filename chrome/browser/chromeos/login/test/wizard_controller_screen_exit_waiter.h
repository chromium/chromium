// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_WIZARD_CONTROLLER_SCREEN_EXIT_WAITER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_WIZARD_CONTROLLER_SCREEN_EXIT_WAITER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/test/test_condition_waiter.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace base {
class RunLoop;
}

namespace chromeos {

// A waiter that blocks until the the current WizardController screen is
// different than the target screen, or the WizardController is destroyed.
class WizardControllerExitWaiter : public test::TestConditionWaiter,
                                   public WizardController::ScreenObserver {
 public:
  explicit WizardControllerExitWaiter(OobeScreenId screen_id);
  explicit WizardControllerExitWaiter(BaseScreen* target_screen);
  ~WizardControllerExitWaiter() override;

  // WizardController::ScreenObserver:
  void OnCurrentScreenChanged(BaseScreen* new_screen) override;
  void OnShutdown() override;

  // TestConditionWaiter;
  void Wait() override;

 private:
  enum class State { IDLE, WAITING_FOR_SCREEN_EXIT, DONE };

  void EndWait();

  const BaseScreen* target_screen_;

  State state_ = State::IDLE;

  ScopedObserver<WizardController, WizardController::ScreenObserver>
      screen_observer_{this};

  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_WIZARD_CONTROLLER_SCREEN_EXIT_WAITER_H_

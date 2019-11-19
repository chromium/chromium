// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_SCREEN_EXIT_WAITER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_SCREEN_EXIT_WAITER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/test/test_condition_waiter.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

namespace base {
class RunLoop;
}

namespace chromeos {

class OobeUI;

// A waiter that blocks until the the current OOBE screen is different than the
// target screen, or the OOBE UI is destroyed.
class OobeScreenExitWaiter : public OobeUI::Observer,
                             public test::TestConditionWaiter {
 public:
  explicit OobeScreenExitWaiter(OobeScreenId target_screen);
  ~OobeScreenExitWaiter() override;

  // OobeUI::Observer implementation:
  void OnCurrentScreenChanged(OobeScreenId current_screen,
                              OobeScreenId new_screen) override;
  void OnDestroyingOobeUI() override;

  // TestConditionWaiter;
  void Wait() override;

 private:
  enum class State { IDLE, WAITING_FOR_SCREEN_EXIT, DONE };

  OobeUI* GetOobeUI();
  void EndWait();

  const OobeScreenId target_screen_;

  State state_ = State::IDLE;

  ScopedObserver<OobeUI, OobeUI::Observer> oobe_ui_observer_{this};

  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(OobeScreenExitWaiter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_SCREEN_EXIT_WAITER_H_

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_OOBE_SCREEN_EXIT_WAITER_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_OOBE_SCREEN_EXIT_WAITER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"

namespace base {
class RunLoop;
}

namespace ash {

// A waiter that blocks until the the current OOBE screen is different than the
// target screen, or the OOBE UI is destroyed.
class OobeScreenExitWaiter : public OobeUI::Observer,
                             public test::TestConditionWaiter {
 public:
  explicit OobeScreenExitWaiter(OobeScreenId target_screen);

  OobeScreenExitWaiter(const OobeScreenExitWaiter&) = delete;
  OobeScreenExitWaiter& operator=(const OobeScreenExitWaiter&) = delete;

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

  base::ScopedObservation<OobeUI, OobeUI::Observer> oobe_ui_observation_{this};

  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_OOBE_SCREEN_EXIT_WAITER_H_

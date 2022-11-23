// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_OOBE_SCREEN_WAITER_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_OOBE_SCREEN_WAITER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace base {
class RunLoop;
}

namespace ash {

// A waiter that blocks until the target oobe screen is reached.
class OobeScreenWaiter : public OobeUI::Observer,
                         public test::TestConditionWaiter,
                         public aura::WindowObserver {
 public:
  explicit OobeScreenWaiter(OobeScreenId target_screen);

  OobeScreenWaiter(const OobeScreenWaiter&) = delete;
  OobeScreenWaiter& operator=(const OobeScreenWaiter&) = delete;

  ~OobeScreenWaiter() override;

  void set_no_assert_last_screen() { assert_last_screen_ = false; }
  void set_assert_next_screen() { assert_next_screen_ = true; }
  void set_no_check_native_window_visible() {
    check_native_window_visible_ = false;
  }

  // OobeUI::Observer implementation:
  void OnCurrentScreenChanged(OobeScreenId current_screen,
                              OobeScreenId new_screen) override;
  void OnDestroyingOobeUI() override;

  // TestConditionWaiter;
  void Wait() override;

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

 private:
  enum class State { IDLE, WAITING_FOR_SCREEN, DONE };

  OobeUI* GetOobeUI();
  bool CheckIfDone();
  void EndWait();

  // Returns true if the target screen is reached.
  bool IsTargetScreenReached();

  // Returns true if the native window is visible.
  bool IsNativeWindowVisible();

  const OobeScreenId target_screen_;

  State state_ = State::IDLE;

  // If set, waiter will assert that the OOBE UI does not transition to any
  // other screen after the transition to the target screen.
  // This applies to the time period Wait() is running only.
  bool assert_last_screen_ = true;

  // If set, the waiter will assert OOBE UI does not transition to any other
  // screen before transitioning to the target screen.
  bool assert_next_screen_ = false;

  // If set, the waiter will only finish waiting if the target screen has been
  // reached and the native window is visible. The assert_last_screen and
  // assert_next_screen checks will only be done if the native window is
  // visible. True by default.
  bool check_native_window_visible_ = true;

  base::ScopedObservation<OobeUI, OobeUI::Observer> oobe_ui_observation_{this};

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      native_window_observation_{this};

  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_OOBE_SCREEN_WAITER_H_

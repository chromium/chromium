// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_LOCK_STATE_CONTROLLER_TEST_API_H_
#define ASH_WM_LOCK_STATE_CONTROLLER_TEST_API_H_

#include "ash/wm/lock_state_controller.h"
#include "base/memory/raw_ptr.h"

namespace ash {

// Helper class used by tests to access LockStateController's internal state.
class LockStateControllerTestApi {
 public:
  explicit LockStateControllerTestApi(LockStateController* controller);

  LockStateControllerTestApi(const LockStateControllerTestApi&) = delete;
  LockStateControllerTestApi& operator=(const LockStateControllerTestApi&) =
      delete;

  ~LockStateControllerTestApi();

  void set_shutdown_controller(ShutdownController* shutdown_controller) {
    controller_->shutdown_controller_ = shutdown_controller;
  }

  bool shutdown_timer_is_running() const {
    return controller_->pre_shutdown_timer_.IsRunning();
  }
  bool real_shutdown_timer_is_running() const {
    return controller_->real_shutdown_timer_.IsRunning();
  }
  bool is_animating_lock() const { return controller_->animating_lock_; }

  void trigger_shutdown_timeout() {
    controller_->OnPreShutdownAnimationTimeout();
    controller_->pre_shutdown_timer_.Stop();
  }
  void trigger_real_shutdown_timeout() {
    controller_->OnRealPowerTimeout();
    controller_->real_shutdown_timer_.Stop();
  }

 private:
  raw_ptr<LockStateController, DanglingUntriaged | ExperimentalAsh>
      controller_;  // not owned
};

}  // namespace ash

#endif  // ASH_WM_LOCK_STATE_CONTROLLER_TEST_API_H_

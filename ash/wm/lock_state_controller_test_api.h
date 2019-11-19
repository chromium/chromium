// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_LOCK_STATE_CONTROLLER_TEST_API_H_
#define ASH_WM_LOCK_STATE_CONTROLLER_TEST_API_H_

#include "ash/wm/lock_state_controller.h"

namespace ash {

// Helper class used by tests to access LockStateController's internal state.
class LockStateControllerTestApi {
 public:
  explicit LockStateControllerTestApi(LockStateController* controller);
  ~LockStateControllerTestApi();

  void set_shutdown_controller(ShutdownController* shutdown_controller) {
    controller_->shutdown_controller_ = shutdown_controller;
  }

  bool lock_fail_timer_is_running() const {
    return controller_->lock_fail_timer_.IsRunning();
  }
  bool shutdown_timer_is_running() const {
    return controller_->pre_shutdown_timer_.IsRunning();
  }
  bool real_shutdown_timer_is_running() const {
    return controller_->real_shutdown_timer_.IsRunning();
  }
  bool is_animating_lock() const { return controller_->animating_lock_; }

  void trigger_lock_fail_timeout() {
    controller_->OnLockFailTimeout();
    controller_->lock_fail_timer_.Stop();
  }
  void trigger_shutdown_timeout() {
    controller_->OnPreShutdownAnimationTimeout();
    controller_->pre_shutdown_timer_.Stop();
  }
  void trigger_real_shutdown_timeout() {
    controller_->OnRealPowerTimeout();
    controller_->real_shutdown_timer_.Stop();
  }

 private:
  LockStateController* controller_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(LockStateControllerTestApi);
};

}  // namespace ash

#endif  // ASH_WM_LOCK_STATE_CONTROLLER_TEST_API_H_

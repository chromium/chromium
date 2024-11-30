// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_IDLE_HANDLER_H_
#define CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_IDLE_HANDLER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"

namespace ash {

class IdleDetector;

// Watch user activity and handle actions for idle for Demo mode. When device is
// idle, reset device states(i.e. close all windows, restart attract loop...)
// and wait for next user.
class DemoModeIdleHandler : public ui::UserActivityObserver {
 public:
  using LaunchDemoAppCallback = base::RepeatingCallback<void()>;

  explicit DemoModeIdleHandler(LaunchDemoAppCallback launch_demo_app_callback);
  DemoModeIdleHandler(const DemoModeIdleHandler&) = delete;
  DemoModeIdleHandler& operator=(const DemoModeIdleHandler&) = delete;
  ~DemoModeIdleHandler() override;

  // ui::UserActivityObserver:
  void OnUserActivity(const ui::Event* event) override;

 private:
  void OnIdle();

  // True when the device is not idle.
  bool is_user_active_ = false;

  // Triggered when the device has been idle for kReLuanchDemoAppIdleDuration
  // seconds.
  LaunchDemoAppCallback launch_demo_app_callback_;

  // Detect idle when attract loop is not playing. If the attract loop is well
  // function and it is not playing, it indicates that a user is actively engage
  // with device.
  std::unique_ptr<IdleDetector> idle_detector_;

  base::ScopedObservation<ui::UserActivityDetector, ui::UserActivityObserver>
      user_activity_observer_{this};

  base::WeakPtrFactory<DemoModeIdleHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_IDLE_HANDLER_H_

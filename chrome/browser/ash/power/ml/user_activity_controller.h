// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_ML_USER_ACTIVITY_CONTROLLER_H_
#define CHROME_BROWSER_ASH_POWER_ML_USER_ACTIVITY_CONTROLLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "chrome/browser/ash/power/ml/idle_event_notifier.h"
#include "chrome/browser/ash/power/ml/user_activity_manager.h"
#include "chrome/browser/ash/power/ml/user_activity_ukm_logger_impl.h"

namespace ash {
namespace power {
namespace ml {

// This controller class sets up and destroys all the components associated with
// user activity logging (IdleEventNotifier, UserActivityUkmLogger and
// UserActivityManager).
class UserActivityController {
 public:
  static UserActivityController* Get();

  UserActivityController();

  UserActivityController(const UserActivityController&) = delete;
  UserActivityController& operator=(const UserActivityController&) = delete;

  ~UserActivityController();

  // Prepares features, makes smart dim decision and returns the result via
  // |callback|.
  void ShouldDeferScreenDim(base::OnceCallback<void(bool)> callback);

 private:
  std::unique_ptr<IdleEventNotifier> idle_event_notifier_;
  UserActivityUkmLoggerImpl user_activity_ukm_logger_;
  std::unique_ptr<UserActivityManager> user_activity_manager_;
};

}  // namespace ml
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_ML_USER_ACTIVITY_CONTROLLER_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_LOGGED_IN_USER_ENABLED_PROVIDER_H_
#define ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_LOGGED_IN_USER_ENABLED_PROVIDER_H_

#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/quick_pair/feature_status_tracker/base_enabled_provider.h"
#include "base/scoped_observation.h"

namespace ash {
namespace quick_pair {

// Observes whether there is a logged in user.
class LoggedInUserEnabledProvider : public BaseEnabledProvider,
                                    public SessionObserver {
 public:
  LoggedInUserEnabledProvider();
  ~LoggedInUserEnabledProvider() override;

  // SessionObserver:
  void OnLoginStatusChanged(LoginStatus login_status) override;

 private:
  base::ScopedObservation<SessionController, SessionObserver> observation_{
      this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_LOGGED_IN_USER_ENABLED_PROVIDER_H_

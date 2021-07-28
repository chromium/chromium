// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_LOGGED_IN_USER_ENABLED_PROVIDER_H_
#define ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_LOGGED_IN_USER_ENABLED_PROVIDER_H_

#include "ash/quick_pair/feature_status_tracker/base_enabled_provider.h"
#include "components/user_manager/user_manager.h"

namespace user_manager {
class User;
}

namespace ash {
namespace quick_pair {

// Observes whether there is a logged in user.
class LoggedInUserEnabledProvider
    : public BaseEnabledProvider,
      public user_manager::UserManager::UserSessionStateObserver {
 public:
  LoggedInUserEnabledProvider();
  ~LoggedInUserEnabledProvider() override;

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_LOGGED_IN_USER_ENABLED_PROVIDER_H_

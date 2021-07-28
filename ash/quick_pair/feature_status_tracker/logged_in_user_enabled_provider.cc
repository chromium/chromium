// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/logged_in_user_enabled_provider.h"

#include "ash/quick_pair/common/logging.h"
#include "base/bind.h"
#include "base/memory/scoped_refptr.h"

namespace ash {
namespace quick_pair {

LoggedInUserEnabledProvider::LoggedInUserEnabledProvider() {
  auto* user_manager = user_manager::UserManager::Get();
  user_manager->AddSessionStateObserver(this);
  SetEnabledAndInvokeCallback(user_manager->IsUserLoggedIn());
}

LoggedInUserEnabledProvider::~LoggedInUserEnabledProvider() {
  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
}

void LoggedInUserEnabledProvider::ActiveUserChanged(
    user_manager::User* active_user) {
  if (active_user) {
    SetEnabledAndInvokeCallback(true);
    return;
  }
  SetEnabledAndInvokeCallback(false);
}

}  // namespace quick_pair
}  // namespace ash

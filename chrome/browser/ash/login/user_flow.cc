// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/user_flow.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "components/account_id/account_id.h"

namespace ash {

UserFlow::UserFlow() {}

UserFlow::~UserFlow() {}

DefaultUserFlow::~DefaultUserFlow() {}

bool DefaultUserFlow::HandleLoginFailure(const AuthFailure& failure) {
  return false;
}

void DefaultUserFlow::HandleLoginSuccess(const UserContext& context) {}

ExtendedUserFlow::ExtendedUserFlow(const AccountId& account_id)
    : account_id_(account_id) {}

ExtendedUserFlow::~ExtendedUserFlow() {}

void ExtendedUserFlow::UnregisterFlowSoon() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ChromeUserManager::ResetUserFlow,
                     base::Unretained(ChromeUserManager::Get()), account_id()));
}

}  // namespace ash

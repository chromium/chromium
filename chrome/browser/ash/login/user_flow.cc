// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/user_flow.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "components/account_id/account_id.h"

namespace chromeos {

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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ChromeUserManager::ResetUserFlow,
                     base::Unretained(ChromeUserManager::Get()), account_id()));
}

}  // namespace chromeos

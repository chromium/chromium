// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_CHROME_USER_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_CHROME_USER_MANAGER_H_

#include "base/containers/flat_set.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager_base.h"

namespace ash {

// Chrome specific interface of the UserManager.
class ChromeUserManager : public user_manager::UserManagerBase {
 public:
  explicit ChromeUserManager(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  ChromeUserManager(const ChromeUserManager&) = delete;
  ChromeUserManager& operator=(const ChromeUserManager&) = delete;

  ~ChromeUserManager() override;

  // Returns current ChromeUserManager or NULL if instance hasn't been
  // yet initialized.
  static ChromeUserManager* Get();
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_CHROME_USER_MANAGER_H_

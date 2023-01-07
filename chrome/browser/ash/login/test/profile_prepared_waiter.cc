// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/profile_prepared_waiter.h"

#include "base/check_op.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"

namespace ash {
namespace test {

ProfilePreparedWaiter::ProfilePreparedWaiter(const AccountId& account_id)
    : account_id_(account_id) {
  ExistingUserController::current_controller()->AddLoginStatusConsumer(this);
}

ProfilePreparedWaiter::~ProfilePreparedWaiter() {
  if (ExistingUserController::current_controller()) {
    ExistingUserController::current_controller()->RemoveLoginStatusConsumer(
        this);
  }
}

// AuthStatusConsumer
void ProfilePreparedWaiter::OnAuthSuccess(const UserContext& user_context) {
  CHECK_EQ(user_context.GetAccountId(), account_id_);
  done_ = true;
  run_loop_.Quit();
}

void ProfilePreparedWaiter::OnAuthFailure(const AuthFailure& error) {
  NOTIMPLEMENTED();
}

void ProfilePreparedWaiter::Wait() {
  if (done_)
    return;

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id_);
  if (user && ProfileHelper::Get()->GetProfileByUser(user)) {
    done_ = true;
    return;
  }
  run_loop_.Run();
  // Need to wait until all other login status consimers finish.
  base::RunLoop().RunUntilIdle();
}

}  // namespace test
}  // namespace ash

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system/user_removal_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace user_removal_manager {

namespace {

// The time that InitiateUserRemoval waits on the passed callback to do a log
// out, otherwise it does the log out itself.
constexpr base::TimeDelta kFailsafeTimerTimeout = base::Seconds(60);

// Override for the LogOut function inside of tests.
base::OnceClosure& GetLogOutOverrideCallbackForTest() {
  static base::NoDestructor<base::OnceClosure> callback;
  return *callback;
}

}  // namespace

bool RemoveUsersIfNeeded() {
  PrefService* local_state = g_browser_process->local_state();
  const PrefService::Preference* pref =
      local_state->FindPreference(prefs::kRemoveUsersRemoteCommand);

  if (pref->IsDefaultValue()) {
    // Nothing to be done.
    return false;
  }

  if (!pref->GetValue()->GetBool()) {
    LOG(ERROR) << "RemoveUsers started and did not finish. Chrome crashed?";
    // Return to avoid crash loop.
    return false;
  }

  local_state->SetBoolean(prefs::kRemoveUsersRemoteCommand, false);
  local_state->CommitPendingWrite();
  // TODO(https://crbug.com/1344832): Emit start metric here.

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  // Make a copy of the list since we'll be removing users (and the list would
  // change underneath us if we used a reference).
  const user_manager::UserList user_list = user_manager->GetUsers();

  for (user_manager::User* user : user_list) {
    user_manager->RemoveUser(
        user->GetAccountId(),
        user_manager::UserRemovalReason::REMOTE_ADMIN_INITIATED);
  }

  // Revert to default value after removal is done.
  local_state->ClearPref(prefs::kRemoveUsersRemoteCommand);

  // TODO(https://crbug.com/1344832): Emit finish metric here.
  return true;
}

void LogOut() {
  auto& log_out_override_callback = GetLogOutOverrideCallbackForTest();
  if (log_out_override_callback) {
    std::move(log_out_override_callback).Run();
    return;
  }
  chrome::AttemptUserExit();
}

void OverrideLogOutForTesting(base::OnceClosure callback) {
  auto& log_out_override_callback = GetLogOutOverrideCallbackForTest();
  log_out_override_callback = std::move(callback);
}

void InitiateUserRemoval(base::OnceClosure on_pref_persisted_callback) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(prefs::kRemoveUsersRemoteCommand, true);

  local_state->CommitPendingWrite(base::BindOnce(
      [](base::OnceClosure on_pref_persisted_callback) {
        // Start the failsafe timer.
        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE, base::BindOnce(&LogOut), kFailsafeTimerTimeout);

        if (on_pref_persisted_callback)
          std::move(on_pref_persisted_callback).Run();
      },
      std::move(on_pref_persisted_callback)));
}

}  // namespace user_removal_manager
}  // namespace ash

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_cryptohome_remover.h"

#include <string>
#include <utility>

#include "ash/components/cryptohome/userdataauth_util.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/ash/app_mode/pref_names.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"

namespace ash {

namespace {

using ::chromeos::UserDataAuthClient;

void ScheduleDelayedCryptohomeRemoval(const AccountId& account_id) {
  PrefService* const local_state = g_browser_process->local_state();
  {
    DictionaryPrefUpdate dict_update(local_state,
                                     prefs::kAllKioskUsersToRemove);
    dict_update->SetKey(cryptohome::Identification(account_id).id(),
                        base::Value(account_id.GetUserEmail()));
  }
  local_state->CommitPendingWrite();
}

void UnscheduleDelayedCryptohomeRemoval(const cryptohome::Identification& id) {
  PrefService* const local_state = g_browser_process->local_state();
  {
    DictionaryPrefUpdate dict_update(local_state,
                                     prefs::kAllKioskUsersToRemove);
    dict_update->RemoveKey(id.id());
  }
  local_state->CommitPendingWrite();
}

void OnRemoveAppCryptohomeComplete(
    const cryptohome::Identification& id,
    base::OnceClosure callback,
    absl::optional<user_data_auth::RemoveReply> reply) {
  cryptohome::MountError error = ReplyToMountError(reply);
  if (error == cryptohome::MOUNT_ERROR_NONE ||
      error == cryptohome::MOUNT_ERROR_USER_DOES_NOT_EXIST) {
    UnscheduleDelayedCryptohomeRemoval(id);
  }
  if (callback)
    std::move(callback).Run();
}

void PerformDelayedCryptohomeRemovals(bool service_is_available) {
  if (!service_is_available) {
    LOG(ERROR) << "Cryptohome client is not avaiable.";
    return;
  }

  PrefService* local_state = g_browser_process->local_state();
  const base::Value* const dict =
      local_state->GetDictionary(prefs::kAllKioskUsersToRemove);
  for (const auto it : dict->DictItems()) {
    std::string app_id;
    if (it.second.is_string())
      app_id = it.second.GetString();
    VLOG(1) << "Removing obsolete cryptohome for " << app_id;

    const cryptohome::Identification cryptohome_id(
        cryptohome::Identification::FromString(it.first));
    cryptohome::AccountIdentifier account_id_proto;
    account_id_proto.set_account_id(cryptohome_id.id());

    user_data_auth::RemoveRequest request;
    *request.mutable_identifier() = account_id_proto;
    UserDataAuthClient::Get()->Remove(
        request, base::BindOnce(&OnRemoveAppCryptohomeComplete, cryptohome_id,
                                base::OnceClosure()));
  }
}

}  // namespace

void KioskCryptohomeRemover::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kAllKioskUsersToRemove);
}

void KioskCryptohomeRemover::RemoveObsoleteCryptohomes() {
  auto* client = UserDataAuthClient::Get();
  client->WaitForServiceToBeAvailable(
      base::BindOnce(&PerformDelayedCryptohomeRemovals));
}

void KioskCryptohomeRemover::CancelDelayedCryptohomeRemoval(
    const AccountId& account_id) {}

void KioskCryptohomeRemover::RemoveCryptohomesAndExitIfNeeded(
    const std::vector<AccountId>& account_ids) {
  base::RepeatingClosure cryptohomes_barrier_closure;
  const user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  AccountId active_account_id;
  if (active_user)
    active_account_id = active_user->GetAccountId();
  if (std::find(account_ids.begin(), account_ids.end(), active_account_id) !=
      account_ids.end()) {
    cryptohomes_barrier_closure = BarrierClosure(
        account_ids.size() - 1, base::BindOnce(&chrome::AttemptUserExit));
  }

  // First schedule cryptohome removal in case there is a power failure during
  // cryptohome calls.
  for (const auto& account_id : account_ids) {
    ScheduleDelayedCryptohomeRemoval(account_id);
  }

  for (auto& account_id : account_ids) {
    if (account_id != active_account_id) {
      user_data_auth::RemoveRequest request;
      const cryptohome::Identification cryptohome_id(account_id);
      request.mutable_identifier()->set_account_id(cryptohome_id.id());
      UserDataAuthClient::Get()->Remove(
          request, base::BindOnce(&OnRemoveAppCryptohomeComplete, cryptohome_id,
                                  cryptohomes_barrier_closure));
    }
  }
}

}  // namespace ash

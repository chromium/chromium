// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_cryptohome_remover.h"

#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/pref_names.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"

namespace chromeos {

namespace {

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
    dict_update->RemoveWithoutPathExpansion(id.id(), nullptr);
  }
  local_state->CommitPendingWrite();
}

// Functions to deal with legacy prefs -- update the current list from the old
// pref values(dict for regular kiosk and list for arc kiosk).
void UpdateFromDictValue(const char* dict_pref_name) {
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* const users_to_remove =
      local_state->GetDictionary(dict_pref_name);
  {
    DictionaryPrefUpdate dict_update(local_state,
                                     prefs::kAllKioskUsersToRemove);
    for (auto& element : *users_to_remove) {
      std::string app_id;
      element.second->GetAsString(&app_id);
      dict_update->SetKey(element.first, base::Value(app_id));
    }
  }
  local_state->ClearPref(dict_pref_name);
  local_state->CommitPendingWrite();
}

void UpdateFromListValue(const std::string& list_pref_name) {
  PrefService* local_state = g_browser_process->local_state();
  const base::ListValue* const users_to_remove =
      local_state->GetList(list_pref_name);
  {
    DictionaryPrefUpdate dict_update(local_state,
                                     prefs::kAllKioskUsersToRemove);
    for (auto& element : *users_to_remove) {
      dict_update->SetKey(element.GetString(), base::Value(""));
    }
  }
  local_state->ClearPref(list_pref_name);
  local_state->CommitPendingWrite();
}

void OnRemoveAppCryptohomeComplete(
    const cryptohome::Identification& id,
    base::OnceClosure callback,
    base::Optional<cryptohome::BaseReply> reply) {
  cryptohome::MountError error = BaseReplyToMountError(reply);
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

  // Legacy: we need to support cases when the prefs are stored in the old
  // format.
  // TODO(crbug.com/1014431): Remove this where the migration is
  // completed.
  UpdateFromDictValue(prefs::kRegularKioskUsersToRemove);
  UpdateFromListValue(prefs::kArcKioskUsersToRemove);

  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* const dict =
      local_state->GetDictionary(prefs::kAllKioskUsersToRemove);
  for (auto& it : *dict) {
    std::string app_id;
    it.second->GetAsString(&app_id);
    VLOG(1) << "Removing obsolete cryptohome for " << app_id;

    const cryptohome::Identification cryptohome_id(
        cryptohome::Identification::FromString(it.first));
    cryptohome::AccountIdentifier account_id_proto;
    account_id_proto.set_account_id(cryptohome_id.id());

    CryptohomeClient::Get()->RemoveEx(
        account_id_proto, base::BindOnce(&OnRemoveAppCryptohomeComplete,
                                         cryptohome_id, base::OnceClosure()));
  }
}

}  // namespace

void KioskCryptohomeRemover::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kAllKioskUsersToRemove);
  registry->RegisterListPref(prefs::kArcKioskUsersToRemove);
  registry->RegisterDictionaryPref(prefs::kRegularKioskUsersToRemove);
}

void KioskCryptohomeRemover::RemoveObsoleteCryptohomes() {
  chromeos::CryptohomeClient* client = chromeos::CryptohomeClient::Get();
  client->WaitForServiceToBeAvailable(
      base::Bind(&PerformDelayedCryptohomeRemovals));
}

void KioskCryptohomeRemover::CancelDelayedCryptohomeRemoval(
    const AccountId& account_id) {}

void KioskCryptohomeRemover::RemoveCryptohomesAndExitIfNeeded(
    const std::vector<AccountId>& account_ids) {
  base::Closure cryptohomes_barrier_closure;
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
      const cryptohome::Identification cryptohome_id(account_id);
      cryptohome::AccountIdentifier account_id_proto;
      account_id_proto.set_account_id(cryptohome_id.id());
      CryptohomeClient::Get()->RemoveEx(
          account_id_proto,
          base::BindOnce(&OnRemoveAppCryptohomeComplete, cryptohome_id,
                         cryptohomes_barrier_closure));
    }
  }
}

}  // namespace chromeos

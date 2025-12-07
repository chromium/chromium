// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_cryptohome_remover.h"

#include <optional>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "chrome/browser/ash/app_mode/pref_names.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/ash/components/cryptohome/error_util.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"

namespace ash {

KioskCryptohomeRemover::KioskCryptohomeRemover(PrefService* local_state)
    : local_state_(CHECK_DEREF(local_state)) {}

KioskCryptohomeRemover::~KioskCryptohomeRemover() = default;

void KioskCryptohomeRemover::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kAllKioskUsersToRemove);
}

void KioskCryptohomeRemover::RemoveObsoleteCryptohomes() {
  auto* client = UserDataAuthClient::Get();
  client->WaitForServiceToBeAvailable(
      base::BindOnce(&KioskCryptohomeRemover::PerformDelayedCryptohomeRemovals,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KioskCryptohomeRemover::CancelDelayedCryptohomeRemoval(
    const AccountId& account_id) {
  UnscheduleDelayedCryptohomeRemoval(cryptohome::Identification(account_id));
}

void KioskCryptohomeRemover::RemoveCryptohomesAndExitIfNeeded(
    const std::vector<AccountId>& account_ids) {
  base::RepeatingClosure cryptohomes_barrier_closure;
  const user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  AccountId active_account_id;
  if (active_user) {
    active_account_id = active_user->GetAccountId();
  }
  if (base::Contains(account_ids, active_account_id)) {
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
          request,
          base::BindOnce(&KioskCryptohomeRemover::OnRemoveAppCryptohomeComplete,
                         weak_ptr_factory_.GetWeakPtr(), cryptohome_id,
                         cryptohomes_barrier_closure));
    }
  }
}

void KioskCryptohomeRemover::PerformDelayedCryptohomeRemovals(
    bool service_is_available) {
  if (!service_is_available) {
    LOG(ERROR) << "Cryptohome client is not avaiable.";
    return;
  }

  const base::Value::Dict& dict =
      local_state_->GetDict(prefs::kAllKioskUsersToRemove);
  for (const auto it : dict) {
    std::string app_id;
    if (it.second.is_string()) {
      app_id = it.second.GetString();
    }
    VLOG(1) << "Removing obsolete cryptohome for " << app_id;

    const cryptohome::Identification cryptohome_id(
        cryptohome::Identification::FromString(it.first));
    cryptohome::AccountIdentifier account_id_proto;
    account_id_proto.set_account_id(cryptohome_id.id());

    user_data_auth::RemoveRequest request;
    *request.mutable_identifier() = account_id_proto;
    UserDataAuthClient::Get()->Remove(
        request,
        base::BindOnce(&KioskCryptohomeRemover::OnRemoveAppCryptohomeComplete,
                       weak_ptr_factory_.GetWeakPtr(), cryptohome_id,
                       base::OnceClosure()));
  }
}

void KioskCryptohomeRemover::OnRemoveAppCryptohomeComplete(
    const cryptohome::Identification& id,
    base::OnceClosure callback,
    std::optional<user_data_auth::RemoveReply> reply) {
  cryptohome::ErrorWrapper error = ReplyToCryptohomeError(reply);
  if (!cryptohome::HasError(error) ||
      cryptohome::ErrorMatches(
          error, user_data_auth::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND)) {
    UnscheduleDelayedCryptohomeRemoval(id);
  }
  if (callback) {
    std::move(callback).Run();
  }
}

void KioskCryptohomeRemover::ScheduleDelayedCryptohomeRemoval(
    const AccountId& account_id) {
  {
    ScopedDictPrefUpdate dict_update(&local_state_.get(),
                                     prefs::kAllKioskUsersToRemove);
    dict_update->Set(cryptohome::Identification(account_id).id(),
                     account_id.GetUserEmail());
  }
  local_state_->CommitPendingWrite();
}

void KioskCryptohomeRemover::UnscheduleDelayedCryptohomeRemoval(
    const cryptohome::Identification& id) {
  {
    ScopedDictPrefUpdate dict_update(&local_state_.get(),
                                     prefs::kAllKioskUsersToRemove);
    dict_update->Remove(id.id());
  }
  local_state_->CommitPendingWrite();
}

}  // namespace ash

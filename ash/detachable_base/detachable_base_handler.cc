// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/detachable_base/detachable_base_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/detachable_base/detachable_base_observer.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/shell.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {

namespace {

// Keys in local state used to persist detachable base pairing state.
// Example detachable base pref value:
// {
//   ash.detachable_base.devices: {
//     user_key_1: {
//       last_used: hex encoded base device ID
//     },
//     user_key_2: {
//        // no detachable_base device info, e.g. the user has not ever used it
//     }
//   }
// }
constexpr char kLastUsedByUserPrefKey[] = "last_used";

std::string GetKeyForPrefs(const AccountId& account_id) {
  if (account_id.HasAccountIdKey())
    return account_id.GetAccountIdKey();
  return account_id.GetUserEmail();
}

}  // namespace

// static
void DetachableBaseHandler::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kDetachableBaseDevices);
}

DetachableBaseHandler::DetachableBaseHandler(PrefService* local_state)
    : local_state_(local_state) {
  if (HammerdClient::Get())  // May be null in tests
    hammerd_observation_.Observe(HammerdClient::Get());
  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();
  power_manager_observation_.Observe(power_manager_client);

  power_manager_client->GetSwitchStates(
      base::BindOnce(&DetachableBaseHandler::OnGotPowerManagerSwitchStates,
                     weak_ptr_factory_.GetWeakPtr()));

  if (SessionController::Get()) {
    session_observation_.Observe(SessionController::Get());
  } else {
    CHECK_IS_TEST();
  }

  if (GetPairingStatus() != DetachableBasePairingStatus::kNone)
    NotifyPairingStatusChanged();
}

DetachableBaseHandler::~DetachableBaseHandler() = default;

void DetachableBaseHandler::AddObserver(DetachableBaseObserver* observer) {
  observers_.AddObserver(observer);
}

void DetachableBaseHandler::RemoveObserver(DetachableBaseObserver* observer) {
  observers_.RemoveObserver(observer);
}

DetachableBasePairingStatus DetachableBaseHandler::GetPairingStatus() const {
  if (!tablet_mode_.has_value() ||
      *tablet_mode_ == chromeos::PowerManagerClient::TabletMode::ON) {
    return DetachableBasePairingStatus::kNone;
  }

  return pairing_status_;
}

bool DetachableBaseHandler::PairedBaseMatchesLastUsedByUser(
    const UserInfo& user) const {
  if (GetPairingStatus() != DetachableBasePairingStatus::kAuthenticated)
    return false;

  DCHECK(!authenticated_base_id_.empty());

  // If local state is not set for non-ephemeral users, the last used detachable
  // base cannot be determined at this point.
  // Assume no device has previously been used - the state will be evaluated
  // again when the local state prefs get initialzized.
  if (!local_state_ && !user.is_ephemeral)
    return true;

  DetachableBaseId last_used_base = GetLastUsedDeviceForUser(user);
  return last_used_base.empty() || authenticated_base_id_ == last_used_base;
}

bool DetachableBaseHandler::SetPairedBaseAsLastUsedByUser(
    const UserInfo& user) {
  if (GetPairingStatus() != DetachableBasePairingStatus::kAuthenticated)
    return false;

  // Do not update the last paired device for non-ephemeral users if local state
  // is not set, as the value would not be correctly preserved. Note that the
  // observers will be notified about the pairing status change when the local
  // state gets initialized - they should attempt to set this value at that
  // point.
  if (!local_state_ && !user.is_ephemeral)
    return false;

  last_used_devices_[user.account_id] = authenticated_base_id_;

  if (!user.is_ephemeral) {
    ScopedDictPrefUpdate update(local_state_, prefs::kDetachableBaseDevices);
    update->EnsureDict(GetKeyForPrefs(user.account_id))
        ->Set(kLastUsedByUserPrefKey, authenticated_base_id_);
  }

  return true;
}

void DetachableBaseHandler::BaseFirmwareUpdateNeeded() {
  NotifyBaseRequiresFirmwareUpdate(true /*requires_update*/);
}

void DetachableBaseHandler::BaseFirmwareUpdateStarted() {}

void DetachableBaseHandler::BaseFirmwareUpdateSucceeded() {
  NotifyBaseRequiresFirmwareUpdate(false /*requires_update*/);
}

void DetachableBaseHandler::BaseFirmwareUpdateFailed() {}

void DetachableBaseHandler::PairChallengeSucceeded(
    const std::vector<uint8_t>& base_id) {
  authenticated_base_id_ = base::HexEncode(base_id);
  pairing_status_ = DetachableBasePairingStatus::kAuthenticated;

  if (GetPairingStatus() != DetachableBasePairingStatus::kNone)
    NotifyPairingStatusChanged();
}

void DetachableBaseHandler::PairChallengeFailed() {
  authenticated_base_id_.clear();
  pairing_status_ = DetachableBasePairingStatus::kNotAuthenticated;

  if (GetPairingStatus() != DetachableBasePairingStatus::kNone)
    NotifyPairingStatusChanged();
}

void DetachableBaseHandler::InvalidBaseConnected() {
  authenticated_base_id_.clear();
  pairing_status_ = DetachableBasePairingStatus::kInvalidDevice;

  if (GetPairingStatus() != DetachableBasePairingStatus::kNone)
    NotifyPairingStatusChanged();
}

void DetachableBaseHandler::TabletModeEventReceived(
    chromeos::PowerManagerClient::TabletMode mode,
    base::TimeTicks timestamp) {
  UpdateTabletMode(mode);
}

void DetachableBaseHandler::OnUserToBeRemoved(const AccountId& account_id) {
  last_used_devices_.erase(account_id);
  ScopedDictPrefUpdate update(local_state_, prefs::kDetachableBaseDevices);
  update->Remove(GetKeyForPrefs(account_id));
}

void DetachableBaseHandler::OnGotPowerManagerSwitchStates(
    std::optional<chromeos::PowerManagerClient::SwitchStates> switch_states) {
  if (!switch_states.has_value() || tablet_mode_.has_value())
    return;

  UpdateTabletMode(switch_states->tablet_mode);
}

void DetachableBaseHandler::UpdateTabletMode(
    chromeos::PowerManagerClient::TabletMode mode) {
  DetachableBasePairingStatus old_pairing_status = GetPairingStatus();
  tablet_mode_ = mode;

  if (*tablet_mode_ == chromeos::PowerManagerClient::TabletMode::ON) {
    authenticated_base_id_.clear();
    pairing_status_ = DetachableBasePairingStatus::kNone;
    NotifyBaseRequiresFirmwareUpdate(false /*requires_update*/);
  }

  if (GetPairingStatus() != old_pairing_status)
    NotifyPairingStatusChanged();
}

DetachableBaseHandler::DetachableBaseId
DetachableBaseHandler::GetLastUsedDeviceForUser(const UserInfo& user) const {
  const auto it = last_used_devices_.find(user.account_id);
  // If the last used device was set within this session, bypass local state.
  if (it != last_used_devices_.end())
    return it->second;

  // For ephemeral lookup do not attempt getting the value from local state;
  // return empty string instead.
  if (user.is_ephemeral)
    return "";

  const base::Value::Dict& detachable_base_info =
      local_state_->GetDict(prefs::kDetachableBaseDevices);
  const base::Value::Dict* account_info =
      detachable_base_info.FindDictByDottedPath(
          GetKeyForPrefs(user.account_id));
  if (!account_info)
    return "";
  const std::string* last_used =
      account_info->FindString(kLastUsedByUserPrefKey);

  return last_used ? *last_used : "";
}

void DetachableBaseHandler::NotifyPairingStatusChanged() {
  for (auto& observer : observers_)
    observer.OnDetachableBasePairingStatusChanged(GetPairingStatus());
}

void DetachableBaseHandler::NotifyBaseRequiresFirmwareUpdate(
    bool requires_update) {
  for (auto& observer : observers_)
    observer.OnDetachableBaseRequiresUpdateChanged(requires_update);
}

}  // namespace ash

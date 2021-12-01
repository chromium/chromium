// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/hps_notify_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "components/account_id/account_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

HpsNotifyController::HpsNotifyController() {
  // When the controller is initialized, we are never in an active user session
  // and we never have any user preferences active. Hence, our default state
  // values are correct.

  // Session controller is instantiated before us in the shell.
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  DCHECK(session_controller);
  session_observation_.Observe(session_controller);

  // Poll the current HPS notify state if the daemon is active. Then, from now
  // on, observe changes to the HPS notify signal.
  chromeos::HpsDBusClient::Get()->GetResultHpsNotify(base::BindOnce(
      &HpsNotifyController::OnHpsPollResult, weak_ptr_factory_.GetWeakPtr()));
  hps_dbus_observation_.Observe(chromeos::HpsDBusClient::Get());
}

HpsNotifyController::~HpsNotifyController() = default;

// static
void HpsNotifyController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kSnoopingProtectionEnabled,
      /*default_value=*/false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

void HpsNotifyController::OnSessionStateChanged(
    session_manager::SessionState session_state) {
  UpdateIconVisibility(session_state == session_manager::SessionState::ACTIVE,
                       hps_state_, is_enabled_);
}

void HpsNotifyController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  DCHECK(pref_service);

  UpdateIconVisibility(
      session_active_, hps_state_,
      pref_service->GetBoolean(prefs::kSnoopingProtectionEnabled));

  // Re-subscribe to pref changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      prefs::kSnoopingProtectionEnabled,
      base::BindRepeating(&HpsNotifyController::OnPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));
}

void HpsNotifyController::OnHpsNotifyChanged(bool hps_state) {
  UpdateIconVisibility(session_active_, hps_state, is_enabled_);
}

void HpsNotifyController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void HpsNotifyController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool HpsNotifyController::IsIconVisible() const {
  return session_active_ && hps_state_ && is_enabled_;
}

void HpsNotifyController::UpdateIconVisibility(bool session_active,
                                               bool hps_state,
                                               bool is_enabled) {
  const bool old_visibility = IsIconVisible();

  session_active_ = session_active;
  hps_state_ = hps_state;
  is_enabled_ = is_enabled;

  const bool new_visibility = IsIconVisible();

  if (old_visibility == new_visibility)
    return;

  for (auto& observer : observers_)
    observer.ShouldUpdateVisibility(new_visibility);
}

void HpsNotifyController::OnHpsPollResult(absl::optional<bool> result) {
  if (!result.has_value()) {
    LOG(WARNING) << "Polling the presence daemon failed";
    return;
  }

  UpdateIconVisibility(session_active_, *result, is_enabled_);
}

void HpsNotifyController::OnPrefChanged() {
  DCHECK(pref_change_registrar_);
  DCHECK(pref_change_registrar_->prefs());

  UpdateIconVisibility(session_active_, hps_state_,
                       pref_change_registrar_->prefs()->GetBoolean(
                           prefs::kSnoopingProtectionEnabled));
}

}  // namespace ash

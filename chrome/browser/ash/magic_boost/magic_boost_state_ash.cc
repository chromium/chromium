// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/magic_boost/magic_boost_state_ash.h"

#include <cstdint>

#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/prefs/pref_service.h"

namespace ash {

MagicBoostStateAsh::MagicBoostStateAsh() {
  shell_observation_.Observe(ash::Shell::Get());

  auto* session_controller = ash::Shell::Get()->session_controller();
  CHECK(session_controller);

  session_observation_.Observe(session_controller);

  // Register pref changes if use session already started.
  if (session_controller->IsActiveUserSessionStarted()) {
    PrefService* prefs = session_controller->GetPrimaryUserPrefService();
    CHECK(prefs);
    RegisterPrefChanges(prefs);
  }
}

MagicBoostStateAsh::~MagicBoostStateAsh() = default;

void MagicBoostStateAsh::OnFirstSessionStarted() {
  PrefService* prefs =
      ash::Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  RegisterPrefChanges(prefs);
}

int32_t MagicBoostStateAsh::AsyncIncrementHMRConsentWindowDismissCount() {
  int32_t incremented_count = hmr_consent_window_dismiss_count() + 1;
  pref_change_registrar_->prefs()->SetInteger(
      ash::prefs::kHMRConsentWindowDismissCount, incremented_count);
  return incremented_count;
}

void MagicBoostStateAsh::AsyncWriteConsentStatus(
    chromeos::HMRConsentStatus consent_status) {
  pref_change_registrar_->prefs()->SetInteger(
      ash::prefs::kHMRConsentStatus, base::to_underlying(consent_status));
}

void MagicBoostStateAsh::OnShellDestroying() {
  session_observation_.Reset();
  shell_observation_.Reset();
}

void MagicBoostStateAsh::RegisterPrefChanges(PrefService* pref_service) {
  pref_change_registrar_.reset();

  if (!pref_service) {
    return;
  }
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      ash::prefs::kHMRConsentStatus,
      base::BindRepeating(&MagicBoostStateAsh::OnHMRConsentStatusUpdated,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      ash::prefs::kHMRConsentWindowDismissCount,
      base::BindRepeating(
          &MagicBoostStateAsh::OnHMRConsentWindowDismissCountUpdated,
          base::Unretained(this)));

  OnHMRConsentStatusUpdated();
  OnHMRConsentWindowDismissCountUpdated();
}

void MagicBoostStateAsh::OnHMRConsentStatusUpdated() {
  auto consent_status = static_cast<chromeos::HMRConsentStatus>(
      pref_change_registrar_->prefs()->GetInteger(
          ash::prefs::kHMRConsentStatus));

  UpdateHMRConsentStatus(consent_status);
}

void MagicBoostStateAsh::OnHMRConsentWindowDismissCountUpdated() {
  UpdateHMRConsentWindowDismissCount(
      pref_change_registrar_->prefs()->GetInteger(
          ash::prefs::kHMRConsentWindowDismissCount));
}

}  // namespace ash

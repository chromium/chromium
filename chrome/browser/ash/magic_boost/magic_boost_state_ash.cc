// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/magic_boost/magic_boost_state_ash.h"

#include <cstdint>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/ash/input_method/editor_mediator_factory.h"
#include "chrome/browser/ash/input_method/editor_panel_manager.h"
#include "chrome/browser/ash/mahi/mahi_availability.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom.h"
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

MagicBoostStateAsh::~MagicBoostStateAsh() {
  editor_manager_for_test_ = nullptr;
}

void MagicBoostStateAsh::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  RegisterPrefChanges(pref_service);
}

bool MagicBoostStateAsh::IsMagicBoostAvailable() {
  return mahi_availability::IsMahiAvailable();
}

bool MagicBoostStateAsh::CanShowNoticeBannerForHMR() {
  PrefService* pref = pref_change_registrar_->prefs();

  // Only show the notice when:
  //  1. HMR is forced ON by the admin, and
  //  2. The consent status is currently disabled.
  return pref->IsManagedPreference(ash::prefs::kHmrEnabled) &&
         pref->GetBoolean(ash::prefs::kHmrEnabled) &&
         hmr_consent_status().has_value() &&
         hmr_consent_status().value() == chromeos::HMRConsentStatus::kDeclined;
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

void MagicBoostStateAsh::AsyncWriteHMREnabled(bool enabled) {
  pref_change_registrar_->prefs()->SetBoolean(ash::prefs::kHmrEnabled, enabled);
}

void MagicBoostStateAsh::ShouldIncludeOrcaInOptIn(
    base::OnceCallback<void(bool)> callback) {
  GetEditorPanelManager()->GetEditorPanelContext(base::BindOnce(
      [](base::OnceCallback<void(bool)> callback,
         crosapi::mojom::EditorPanelContextPtr panel_context) {
        // If the mode is not `kHardBlocked` and consent status is not set, it
        // means that we should include Orca in this opt-in flow.
        bool should_include_orca =
            panel_context->editor_panel_mode !=
                crosapi::mojom::EditorPanelMode::kHardBlocked &&
            !panel_context->consent_status_settled;
        std::move(callback).Run(should_include_orca);
      },
      std::move(callback)));
}

void MagicBoostStateAsh::DisableOrcaFeature() {
  GetEditorPanelManager()->OnMagicBoostPromoCardDeclined();
}

void MagicBoostStateAsh::EnableOrcaFeature() {
  // Note that we just need to change consent status to enable the Orca feature,
  // since when Orca consent status is unset, `kOrcaEnabled` should be enabled
  // by default.
  GetEditorPanelManager()->OnConsentApproved();
}

input_method::EditorPanelManager* MagicBoostStateAsh::GetEditorPanelManager() {
  if (editor_manager_for_test_) {
    return editor_manager_for_test_;
  }

  return input_method::EditorMediatorFactory::GetInstance()
      ->GetForProfile(ProfileManager::GetActiveUserProfile())
      ->panel_manager();
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
      ash::prefs::kMagicBoostEnabled,
      base::BindRepeating(&MagicBoostStateAsh::OnMagicBoostEnabledUpdated,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      ash::prefs::kHmrEnabled,
      base::BindRepeating(&MagicBoostStateAsh::OnHMREnabledUpdated,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      ash::prefs::kHMRConsentStatus,
      base::BindRepeating(&MagicBoostStateAsh::OnHMRConsentStatusUpdated,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      ash::prefs::kHMRConsentWindowDismissCount,
      base::BindRepeating(
          &MagicBoostStateAsh::OnHMRConsentWindowDismissCountUpdated,
          base::Unretained(this)));

  // Initializes the `magic_boost_enabled_` based on the current prefs settings.
  UpdateMagicBoostEnabled(pref_change_registrar_->prefs()->GetBoolean(
      ash::prefs::kMagicBoostEnabled));

  OnHMREnabledUpdated();
  OnHMRConsentStatusUpdated();
  OnHMRConsentWindowDismissCountUpdated();
}

void MagicBoostStateAsh::OnMagicBoostEnabledUpdated() {
  bool enabled = pref_change_registrar_->prefs()->GetBoolean(
      ash::prefs::kMagicBoostEnabled);

  UpdateMagicBoostEnabled(enabled);

  // Update both HMR and Orca accordingly when `kMagicBoostEnabled` is changed.
  AsyncWriteHMREnabled(enabled);
  pref_change_registrar_->prefs()->SetBoolean(ash::prefs::kOrcaEnabled,
                                              enabled);
}

void MagicBoostStateAsh::OnHMREnabledUpdated() {
  bool enabled =
      pref_change_registrar_->prefs()->GetBoolean(ash::prefs::kHmrEnabled);

  UpdateHMREnabled(enabled);

  auto consent_status =
      hmr_consent_status().value_or(chromeos::HMRConsentStatus::kApproved);

  // The feature can be enabled through the Settings page. In that case,
  // `consent_status` can be unset or declined, and we need to flip it to
  // `kPending` so that when users try to access the feature, we would show the
  // disclaimer UI.
  if (enabled && (consent_status == chromeos::HMRConsentStatus::kUnset ||
                  consent_status == chromeos::HMRConsentStatus::kDeclined)) {
    AsyncWriteConsentStatus(chromeos::HMRConsentStatus::kPendingDisclaimer);
  }
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

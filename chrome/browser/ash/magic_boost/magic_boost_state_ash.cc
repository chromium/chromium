// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/magic_boost/magic_boost_state_ash.h"

#include <cstdint>
#include <memory>
#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/mahi/mahi_utils.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/scoped_observation.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/ash/input_method/editor_mediator_factory.h"
#include "chrome/browser/ash/input_method/editor_panel_manager.h"
#include "chrome/browser/ash/mahi/mahi_availability.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_context.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_mode.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace {

// Wait for refresh tokens load and run the provided callback. This object
// immediately runs the callback if refresh tokens are already loaded.
class RefreshTokensLoadedBarrier : public signin::IdentityManager::Observer {
 public:
  RefreshTokensLoadedBarrier(Profile* profile,
                             signin::IdentityManager* identity_manager,
                             base::OnceCallback<void()> callback)
      : callback_(std::move(callback)) {
    CHECK(profile);
    CHECK(identity_manager);

    if (identity_manager->AreRefreshTokensLoaded()) {
      std::move(callback_).Run();
      return;
    }

    identity_manager_observation_.Observe(identity_manager);
  }

  void OnRefreshTokensLoaded() override {
    std::move(callback_).Run();
    identity_manager_observation_.Reset();
  }

 private:
  base::OnceCallback<void()> callback_;
  base::ScopedObservation<signin::IdentityManager, RefreshTokensLoadedBarrier>
      identity_manager_observation_{this};
};

}  // namespace

MagicBoostStateAsh::MagicBoostStateAsh()
    : MagicBoostStateAsh(
          MagicBoostStateAsh::InjectActiveProfileForTestingCallback()) {}

MagicBoostStateAsh::MagicBoostStateAsh(
    MagicBoostStateAsh::InjectActiveProfileForTestingCallback
        inject_active_profile_for_testing_callback)
    : inject_active_profile_for_testing_callback_(
          inject_active_profile_for_testing_callback) {
  if (!inject_active_profile_for_testing_callback_.is_null()) {
    CHECK_IS_TEST();
  }

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

Profile* MagicBoostStateAsh::GetActiveUserProfile() {
  if (!inject_active_profile_for_testing_callback_.is_null()) {
    CHECK_IS_TEST();
    return inject_active_profile_for_testing_callback_.Run();
  }

  return Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(
          user_manager::UserManager::Get()->GetActiveUser()));
}

void MagicBoostStateAsh::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  RegisterPrefChanges(pref_service);
}

bool MagicBoostStateAsh::CanShowNoticeBannerForHMR() {
  PrefService* pref = pref_change_registrar_->prefs();

  // TODO(b:397521071): now the kHmrEnabled is not managed, this logic needs to
  // be revisited.
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
         const chromeos::editor_menu::EditorContext& editor_context) {
        // If the mode is not `kHardBlocked` and consent status is not set, it
        // means that we should include Orca in this opt-in flow.
        bool should_include_orca =
            editor_context.mode !=
                chromeos::editor_menu::EditorMode::kHardBlocked &&
            !editor_context.consent_status_settled;
        std::move(callback).Run(should_include_orca);
      },
      std::move(callback)));
}

bool MagicBoostStateAsh::ShouldIncludeOrcaInOptInSync() {
  return GetEditorPanelManager()->ShouldOptInEditor();
}

void MagicBoostStateAsh::DisableOrcaFeature() {
  GetEditorPanelManager()->OnMagicBoostPromoCardDeclined();
}

void MagicBoostStateAsh::DisableLobsterSettings() {
  pref_change_registrar_->prefs()->SetBoolean(ash::prefs::kLobsterEnabled,
                                              false);
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
      ->GetForProfile(GetActiveUserProfile())
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
      ash::prefs::kHmrManagedSettings,
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

  Profile* profile = GetActiveUserProfile();
  if (!profile) {
    CHECK_IS_TEST();
    // Test code can bypass availability check by a flag. Run check immediately
    // for that case if `profile` is nullptr.
    OnRefreshTokensReady();
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    // `identity_manager` is not available under a certain condition, e.g.,
    // guest session, test code. Run check immediately for those cases.
    OnRefreshTokensReady();
    return;
  }

  // Availability check contains an async operation where value is unavailable
  // until refresh tokens are loaded. Run availability check after refresh token
  // is loaded.
  refresh_tokens_loaded_barrier_.reset(new RefreshTokensLoadedBarrier(
      profile, identity_manager,
      base::BindOnce(&MagicBoostStateAsh::OnRefreshTokensReady,
                     base::Unretained(this))));
}

base::expected<bool, chromeos::MagicBoostState::Error>
MagicBoostStateAsh::IsUserEligibleForGenAIFeaturesExpected() const {
  return mahi_availability::IsMahiAvailable().transform_error(
      [](mahi_availability::Error error) {
        // Use switch statement to get a compile error if new error types are
        // added to mahi_availability::Error.
        switch (error) {
          case mahi_availability::Error::kMantaFeatureBitNotReady:
            return chromeos::MagicBoostState::Error::kUninitialized;
        }
        CHECK(false) << "Unknown mahi_availability Error enum is passed";
      });
}

void MagicBoostStateAsh::OnRefreshTokensReady() {
  ASSIGN_OR_RETURN(bool available, IsUserEligibleForGenAIFeaturesExpected(),
                   [](auto) {});
  UpdateUserEligibleForGenAIFeatures(available);
}

void MagicBoostStateAsh::OnMagicBoostEnabledUpdated() {
  bool enabled = pref_change_registrar_->prefs()->GetBoolean(
      ash::prefs::kMagicBoostEnabled);

  UpdateMagicBoostEnabled(enabled);

  // Update both HMR, Orca and Lobster accordingly when `kMagicBoostEnabled` is
  // changed.
  AsyncWriteHMREnabled(enabled);
  pref_change_registrar_->prefs()->SetBoolean(ash::prefs::kOrcaEnabled,
                                              enabled);
  pref_change_registrar_->prefs()->SetBoolean(ash::prefs::kLobsterEnabled,
                                              enabled);
}

void MagicBoostStateAsh::OnHMREnabledUpdated() {
  // Looks up both the enterprise policy controlled pref and the user controlled
  // pref.
  PrefService* prefs = pref_change_registrar_->prefs();
  bool enabled =
      prefs->GetInteger(ash::prefs::kHmrManagedSettings) !=
          static_cast<int>(mahi_utils::HmrEnterprisePolicy::kDisallowed) &&
      prefs->GetBoolean(ash::prefs::kHmrEnabled);

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

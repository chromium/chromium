// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_enabling.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_user_status_code.h"
#include "chrome/browser/glic/glic_user_status_fetcher.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace glic {

GlicEnabling::ProfileEnablement GlicEnabling::EnablementForProfile(
    Profile* profile) {
  ProfileEnablement result;

  if (!IsEnabledByFlags()) {
    result.feature_disabled = true;
    return result;
  }

  if (!profile || !profile->IsRegularProfile()) {
    result.not_regular_profile = true;
    return result;
  }

  // Certain checks are bypassed if --glic-dev is passed.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(::switches::kGlicDev)) {
    if (!base::FeatureList::IsEnabled(features::kGlicRollout) &&
        !IsEligibleForGlicTieredRollout(profile)) {
      result.not_rolled_out = true;
    }

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    CHECK(identity_manager);
    AccountInfo primary_account =
        identity_manager->FindExtendedAccountInfoByAccountId(
            identity_manager->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin));

    // Not having a primary account is considered ineligible, as is kUnknown
    // for the required account capability.
    if (primary_account.IsEmpty() ||
        primary_account.capabilities.can_use_model_execution_features() !=
            signin::Tribool::kTrue) {
      result.primary_account_not_capable = true;
    }
  }

  if (profile->GetPrefs()->GetInteger(::prefs::kGeminiSettings) !=
      static_cast<int>(glic::prefs::SettingsPolicyState::kEnabled)) {
    result.disallowed_by_chrome_policy = true;
  }

  if (base::FeatureList::IsEnabled(features::kGlicUserStatusCheck)) {
    if (auto cached_user_status =
            GlicUserStatusFetcher::GetCachedUserStatus(profile);
        cached_user_status.has_value()) {
      switch (cached_user_status->user_status_code) {
        case UserStatusCode::DISABLED_BY_ADMIN:
          result.disallowed_by_remote_admin = true;
          break;
        case UserStatusCode::DISABLED_OTHER:
          result.disallowed_by_remote_other = true;
          break;
        case UserStatusCode::ENABLED:
          break;
        case UserStatusCode::SERVER_UNAVAILABLE:
          // We never cache SERVER_UNAVAILABLE.
          NOTREACHED();
      }
    }
  }

  if (!HasConsentedForProfile(profile)) {
    result.not_consented = true;
  }

  return result;
}

bool GlicEnabling::IsEnabledByFlags() {
  // Check that the feature flags are enabled.
  return base::FeatureList::IsEnabled(features::kGlic) &&
         features::IsTabSearchMoving();
}

bool GlicEnabling::IsProfileEligible(const Profile* profile) {
  // Glic is supported only in regular profiles, i.e. disable in incognito,
  // guest, system profile, etc.
  return IsEnabledByFlags() && profile && profile->IsRegularProfile();
}

bool GlicEnabling::IsEnabledForProfile(Profile* profile) {
  return EnablementForProfile(profile).IsEnabled();
}

bool GlicEnabling::HasConsentedForProfile(Profile* profile) {
  return profile->GetPrefs()->GetInteger(prefs::kGlicCompletedFre) ==
         static_cast<int>(prefs::FreStatus::kCompleted);
}

bool GlicEnabling::IsEnabledAndConsentForProfile(Profile* profile) {
  return EnablementForProfile(profile).IsEnabledAndConsented();
}

bool GlicEnabling::DidDismissForProfile(Profile* profile) {
  return profile->GetPrefs()->GetInteger(glic::prefs::kGlicCompletedFre) ==
         static_cast<int>(prefs::FreStatus::kIncomplete);
}

bool GlicEnabling::IsReadyForProfile(Profile* profile) {
  return GetProfileReadyState(profile) == mojom::ProfileReadyState::kReady;
}

mojom::ProfileReadyState GlicEnabling::GetProfileReadyState(Profile* profile) {
  if (!IsEnabledAndConsentForProfile(profile)) {
    return mojom::ProfileReadyState::kUnknownError;
  }

  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kGlicAutomation)) {
    return mojom::ProfileReadyState::kReady;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  // Check that profile is not currently paused.
  CoreAccountInfo core_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (core_account_info.IsEmpty()) {
    return mojom::ProfileReadyState::kUnknownError;
  }
  if (identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          core_account_info.account_id)) {
    return mojom::ProfileReadyState::kSignInRequired;
  }
  return mojom::ProfileReadyState::kReady;
}

bool GlicEnabling::IsEligibleForGlicTieredRollout(Profile* profile) {
  return base::FeatureList::IsEnabled(features::kGlicTieredRollout) &&
         profile->GetPrefs()->GetBoolean(prefs::kGlicRolloutEligibility);
}

bool GlicEnabling::ShouldShowSettingsPage(Profile* profile) {
  return EnablementForProfile(profile).ShouldShowSettingsPage();
}

void GlicEnabling::OnGlicSettingsPolicyChanged() {
  glic::prefs::SettingsPolicyState updated_gemini_settings_value =
      glic::prefs::SettingsPolicyState{
          profile_->GetPrefs()->GetInteger(::prefs::kGeminiSettings)};

  // If the policy changed from either not set or Disabled to Enabled, trigger a
  // rpc fetch to update the possible user status change sooner.
  if ((!cached_gemini_settings_value_.has_value() ||
       cached_gemini_settings_value_.value() ==
           glic::prefs::SettingsPolicyState::kDisabled) &&
      updated_gemini_settings_value ==
          (glic::prefs::SettingsPolicyState::kEnabled)) {
    if (glic_user_status_fetcher_) {
      glic_user_status_fetcher_->UpdateUserStatus();
    }
  }
  // Update the stored value for the next comparison.
  cached_gemini_settings_value_ = updated_gemini_settings_value;

  // Update the overall enabled status as the policy has changed.
  UpdateEnabledStatus();
}

GlicEnabling::GlicEnabling(Profile* profile,
                           ProfileAttributesStorage* profile_attributes_storage)
    : profile_(profile),
      profile_attributes_storage_(profile_attributes_storage) {
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(
      ::prefs::kGeminiSettings,
      base::BindRepeating(&GlicEnabling::OnGlicSettingsPolicyChanged,
                          base::Unretained(this)));
  pref_registrar_.Add(prefs::kGlicCompletedFre,
                      base::BindRepeating(&GlicEnabling::UpdateConsentStatus,
                                          base::Unretained(this)));
  if (!base::FeatureList::IsEnabled(features::kGlicRollout) &&
      base::FeatureList::IsEnabled(features::kGlicTieredRollout)) {
    pref_registrar_.Add(
        prefs::kGlicRolloutEligibility,
        base::BindRepeating(&GlicEnabling::OnTieredRolloutStatusMaybeChanged,
                            base::Unretained(this)));
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CHECK(identity_manager);
  identity_manager_observation_.Observe(identity_manager);

  if (base::FeatureList::IsEnabled(features::kGlicUserStatusCheck)) {
    glic_user_status_fetcher_ = std::make_unique<GlicUserStatusFetcher>(
        profile_, base::BindRepeating(&GlicEnabling::UpdateEnabledStatus,
                                      base::Unretained(this)));
    cached_gemini_settings_value_ = glic::prefs::SettingsPolicyState{
        profile_->GetPrefs()->GetInteger(::prefs::kGeminiSettings)};
  }
}
GlicEnabling::~GlicEnabling() = default;

bool GlicEnabling::IsAllowed() {
  return IsEnabledForProfile(profile_);
}

bool GlicEnabling::HasConsented() {
  return HasConsentedForProfile(profile_);
}

base::CallbackListSubscription GlicEnabling::RegisterAllowedChanged(
    EnableChangedCallback callback) {
  return enable_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription GlicEnabling::RegisterOnConsentChanged(
    ConsentChangedCallback callback) {
  return consent_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription GlicEnabling::RegisterOnShowSettingsPageChanged(
    ShowSettingsPageChangedCallback callback) {
  return show_settings_page_changed_callback_list_.Add(std::move(callback));
}

void GlicEnabling::UpdateUserStatus(
    const signin::PrimaryAccountChangeEvent& event_details) {
  if (!glic_user_status_fetcher_) {
    return;
  }

  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      if (glic_user_status_fetcher_) {
        glic_user_status_fetcher_->UpdateUserStatus();
      }
      break;
    // Ignore until primary account is set.
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      if (glic_user_status_fetcher_) {
        glic_user_status_fetcher_->InvalidateCachedStatus();
        glic_user_status_fetcher_->CancelUserStatusUpdateIfNeeded();
      }
      break;
  }
}
void GlicEnabling::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  UpdateEnabledStatus();
  UpdateUserStatus(event_details);
}

void GlicEnabling::OnExtendedAccountInfoUpdated(const AccountInfo& info) {
  UpdateEnabledStatus();
}

void GlicEnabling::OnExtendedAccountInfoRemoved(const AccountInfo& info) {
  UpdateEnabledStatus();
}

void GlicEnabling::OnRefreshTokensLoaded() {
  UpdateEnabledStatus();
}

// It happens that when the request is sent upon sign-in, the refresh token is
// not available yet, the request would hence be cancelled. In such cases, we
// re-send the request when refresh token becomes available.
void GlicEnabling::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (glic_user_status_fetcher_) {
    glic_user_status_fetcher_->UpdateUserStatusIfNeeded();
  }
}

void GlicEnabling::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  UpdateEnabledStatus();
}

void GlicEnabling::OnTieredRolloutStatusMaybeChanged() {
  UpdateEnabledStatus();
}

void GlicEnabling::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  // Check that the account info here is the same as the primary account, and
  // ignore all events that are not about the primary account.
  if (identity_manager_observation_.GetSource()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin) != account_info) {
    return;
  }
  UpdateEnabledStatus();

  if (glic_user_status_fetcher_) {
    glic_user_status_fetcher_->CancelUserStatusUpdateIfNeeded();
  }
}

void GlicEnabling::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  if (glic_user_status_fetcher_) {
    glic_user_status_fetcher_->CancelUserStatusUpdateIfNeeded();
  }
}

void GlicEnabling::UpdateEnabledStatus() {
  if (ProfileAttributesEntry* entry =
          profile_attributes_storage_->GetProfileAttributesWithPath(
              profile_->GetPath())) {
    entry->SetIsGlicEligible(IsAllowed());
  }
  enable_changed_callback_list_.Notify();
  show_settings_page_changed_callback_list_.Notify();
}

void GlicEnabling::UpdateConsentStatus() {
  consent_changed_callback_list_.Notify();
  show_settings_page_changed_callback_list_.Notify();
}

}  // namespace glic

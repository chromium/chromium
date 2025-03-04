// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_enabling.h"

#include "base/command_line.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace glic {

namespace {
bool IsNonEnterpriseEnabled(Profile* profile) {
  if (!GlicEnabling::IsProfileEligible(profile)) {
    return false;
  }

  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kGlicDev)) {
    return true;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CHECK(identity_manager);
  AccountInfo primary_account =
      identity_manager->FindExtendedAccountInfoByAccountId(
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin));

  // Not having a primary account is considered ineligible.
  if (primary_account.IsEmpty()) {
    return false;
  }

  // Treat `signin::Tribool::kUnknown` as false.
  if (primary_account.capabilities.can_use_model_execution_features() !=
      signin::Tribool::kTrue) {
    return false;
  }

  return true;
}

bool IsEnterpriseEnabled(Profile* profile) {
  return profile->GetPrefs()->GetInteger(::prefs::kGeminiSettings) ==
         static_cast<int>(glic::prefs::SettingsPolicyState::kEnabled);
}
}  // namespace

bool GlicEnabling::IsEnabledByFlags() {
  // Check that the feature flags are enabled.
  return base::FeatureList::IsEnabled(features::kGlic) &&
         base::FeatureList::IsEnabled(features::kTabstripComboButton);
}

bool GlicEnabling::IsProfileEligible(const Profile* profile) {
  // Glic is supported only in regular profiles, i.e. disable in incognito,
  // guest, system profile, etc.
  return IsEnabledByFlags() && profile && profile->IsRegularProfile();
}

bool GlicEnabling::IsEnabledForProfile(Profile* profile) {
  return IsNonEnterpriseEnabled(profile) && IsEnterpriseEnabled(profile);
}

bool GlicEnabling::IsEnabledAndConsentForProfile(Profile* profile) {
  if (!IsEnabledForProfile(profile)) {
    return false;
  }
  return profile->GetPrefs()->GetBoolean(glic::prefs::kGlicCompletedFre);
}

bool GlicEnabling::IsReadyForProfile(Profile* profile) {
  if (!IsEnabledAndConsentForProfile(profile)) {
    return false;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  // Check that profile is not currently paused.
  CoreAccountInfo core_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  return !core_account_info.IsEmpty() &&
         !identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
             core_account_info.account_id);
}

bool GlicEnabling::ShouldShowSettingsPage(Profile* profile) {
  if (!IsEnterpriseEnabled(profile)) {
    // If the feature is disabled by enterprise policy, the settings page should
    // be shown (it will be shown in a policy-disabled state) only if all other
    // non-enterprise conditions are met: the account has all appropriate
    // permissions and has previously completed the FRE before the policy went
    // into effect.
    return IsNonEnterpriseEnabled(profile) &&
           profile->GetPrefs()->GetBoolean(glic::prefs::kGlicCompletedFre);
  }

  return IsEnabledAndConsentForProfile(profile);
}

GlicEnabling::GlicEnabling(Profile* profile) : profile_(profile) {
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(
      ::prefs::kGeminiSettings,
      base::BindRepeating(&GlicEnabling::OnGlicSettingsPolicyChanged,
                          base::Unretained(this)));
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CHECK(identity_manager);
  identity_manager_observation_.Observe(identity_manager);
}
GlicEnabling::~GlicEnabling() = default;

bool GlicEnabling::IsEnabled() {
  return IsEnabledForProfile(profile_);
}

base::CallbackListSubscription GlicEnabling::RegisterEnableChanged(
    EnableChangedCallback callback) {
  return enable_changed_callback_list_.Add(std::move(callback));
}

void GlicEnabling::OnGlicSettingsPolicyChanged() {
  enable_changed_callback_list_.Notify();
}

void GlicEnabling::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  enable_changed_callback_list_.Notify();
}

void GlicEnabling::OnExtendedAccountInfoUpdated(const AccountInfo& info) {
  enable_changed_callback_list_.Notify();
}

void GlicEnabling::OnRefreshTokensLoaded() {
  enable_changed_callback_list_.Notify();
}

void GlicEnabling::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  enable_changed_callback_list_.Notify();
}

}  // namespace glic

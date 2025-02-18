// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_enabling.h"

#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace glic {

bool GlicEnabling::IsEnabledByFlags() {
  // Check that the feature flags are enabled.
  return base::FeatureList::IsEnabled(features::kGlic) &&
         base::FeatureList::IsEnabled(features::kTabstripComboButton);
}

bool GlicEnabling::IsProfileEligible(const Profile* profile) {
  CHECK(profile);
  // Glic is supported only in regular profiles, i.e. disable in incognito,
  // guest, system profile, etc.
  return IsEnabledByFlags() && profile->IsRegularProfile();
}

bool GlicEnabling::IsEnabledForProfile(const Profile* profile) {
  if (!IsProfileEligible(profile)) {
    return false;
  }

  return profile->GetPrefs()->GetInteger(glic::prefs::kGlicSettingsPolicy) ==
         static_cast<int>(glic::prefs::SettingsPolicyState::kEnabled);
}

bool GlicEnabling::IsEnabledAndConsentForProfile(const Profile* profile) {
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

GlicEnabling::GlicEnabling(Profile* profile) : profile_(profile) {
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(
      prefs::kGlicSettingsPolicy,
      base::BindRepeating(&GlicEnabling::OnGlicSettingsPolicyChanged,
                          base::Unretained(this)));
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

}  // namespace glic

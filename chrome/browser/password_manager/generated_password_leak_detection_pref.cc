// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/generated_password_leak_detection_pref.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace {

// Returns whether the user can use the leak detection feature.
bool IsUserAllowedToUseLeakDetection(Profile* profile) {
  return !profile->IsGuestSession() &&
         IdentityManagerFactory::GetForProfileIfExists(profile);
}

// Returns whether the effective value of the Safe Browsing preferences for
// |profile| is standard protection.
bool IsSafeBrowsingStandard(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled) &&
         !profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnhanced);
}

}  // namespace

namespace settings_api = extensions::api::settings_private;

const char kGeneratedPasswordLeakDetectionPref[] =
    "generated.password_leak_detection";

GeneratedPasswordLeakDetectionPref::GeneratedPasswordLeakDetectionPref(
    Profile* profile)
    : profile_(profile) {
  user_prefs_registrar_.Init(profile->GetPrefs());
  user_prefs_registrar_.Add(
      password_manager::prefs::kPasswordLeakDetectionEnabled,
      base::BindRepeating(
          &GeneratedPasswordLeakDetectionPref::OnSourcePreferencesChanged,
          base::Unretained(this)));
  user_prefs_registrar_.Add(
      prefs::kSafeBrowsingEnabled,
      base::BindRepeating(
          &GeneratedPasswordLeakDetectionPref::OnSourcePreferencesChanged,
          base::Unretained(this)));
  user_prefs_registrar_.Add(
      prefs::kSafeBrowsingEnhanced,
      base::BindRepeating(
          &GeneratedPasswordLeakDetectionPref::OnSourcePreferencesChanged,
          base::Unretained(this)));

  if (auto* identity_manager = IdentityManagerFactory::GetForProfile(profile)) {
    identity_manager_observer_.Observe(identity_manager);
  }

  if (auto* sync_service = SyncServiceFactory::GetForProfile(profile)) {
    sync_service_observer_.Observe(sync_service);
  }
}

GeneratedPasswordLeakDetectionPref::~GeneratedPasswordLeakDetectionPref() =
    default;

extensions::settings_private::SetPrefResult
GeneratedPasswordLeakDetectionPref::SetPref(const base::Value* value) {
  if (!value->is_bool()) {
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;
  }

  if (!IsSafeBrowsingStandard(profile_) ||
      !IsUserAllowedToUseLeakDetection(profile_)) {
    return extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;
  }

  if (!profile_->GetPrefs()
           ->FindPreference(
               password_manager::prefs::kPasswordLeakDetectionEnabled)
           ->IsUserModifiable()) {
    return extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;
  }

  profile_->GetPrefs()->SetBoolean(
      password_manager::prefs::kPasswordLeakDetectionEnabled, value->GetBool());

  return extensions::settings_private::SetPrefResult::SUCCESS;
}

settings_api::PrefObject GeneratedPasswordLeakDetectionPref::GetPrefObject()
    const {
  auto* backing_preference = profile_->GetPrefs()->FindPreference(
      password_manager::prefs::kPasswordLeakDetectionEnabled);

  settings_api::PrefObject pref_object;
  pref_object.key = kGeneratedPasswordLeakDetectionPref;
  pref_object.type = settings_api::PrefType::kBoolean;
  pref_object.value = base::Value(backing_preference->GetValue()->GetBool() &&
                                  IsUserAllowedToUseLeakDetection(profile_));
  pref_object.user_control_disabled =
      !IsSafeBrowsingStandard(profile_) ||
      !IsUserAllowedToUseLeakDetection(profile_);
  if (!backing_preference->IsUserModifiable()) {
    pref_object.enforcement = settings_api::Enforcement::kEnforced;
    extensions::settings_private::GeneratedPref::ApplyControlledByFromPref(
        &pref_object, backing_preference);
  } else if (backing_preference->GetRecommendedValue()) {
    pref_object.enforcement = settings_api::Enforcement::kRecommended;
    pref_object.recommended_value =
        base::Value(backing_preference->GetRecommendedValue()->GetBool());
  }

  return pref_object;
}

void GeneratedPasswordLeakDetectionPref::OnSourcePreferencesChanged() {
  NotifyObservers(kGeneratedPasswordLeakDetectionPref);
}

void GeneratedPasswordLeakDetectionPref::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  identity_manager_observer_.Reset();
}

void GeneratedPasswordLeakDetectionPref::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      NotifyObservers(kGeneratedPasswordLeakDetectionPref);
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

void GeneratedPasswordLeakDetectionPref::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  NotifyObservers(kGeneratedPasswordLeakDetectionPref);
}

void GeneratedPasswordLeakDetectionPref::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  NotifyObservers(kGeneratedPasswordLeakDetectionPref);
}

void GeneratedPasswordLeakDetectionPref::OnStateChanged(
    syncer::SyncService* sync) {
  NotifyObservers(kGeneratedPasswordLeakDetectionPref);
}

void GeneratedPasswordLeakDetectionPref::OnSyncCycleCompleted(
    syncer::SyncService* sync) {
  // The base implementation of this calls OnStateChanged, however the pref will
  // only change based on events reported directly to OnStateChanged, and so
  // calling it here is unrequired and causes observer noise.
}

void GeneratedPasswordLeakDetectionPref::OnSyncShutdown(
    syncer::SyncService* sync) {
  sync_service_observer_.Reset();
}

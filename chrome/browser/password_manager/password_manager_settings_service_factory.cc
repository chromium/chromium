// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_settings_service_factory.h"

#include "base/trace_event/trace_event.h"
#include "chrome/browser/password_manager/password_manager_settings_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/password_manager/core/browser/password_manager_settings_service.h"
#include "components/password_manager/core/common/password_manager_features.h"
#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/password_manager/android/password_manager_settings_service_android_impl.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#endif

// static
PasswordManagerSettingsService*
PasswordManagerSettingsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<PasswordManagerSettingsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PasswordManagerSettingsServiceFactory*
PasswordManagerSettingsServiceFactory::GetInstance() {
  return base::Singleton<PasswordManagerSettingsServiceFactory>::get();
}

PasswordManagerSettingsServiceFactory::PasswordManagerSettingsServiceFactory()
    : ProfileKeyedServiceFactory(
          "PasswordManagerSettingsService",
          // On Android, the sync service is used to read prefs and checking
          // them
          // depends on the sync status, thus the service needs to be accessed
          // as for the regular profile. On desktop, the sync service is not
          // used, but since this service is used to access settings which are
          // not specific to incognito the service can still be used as for the
          // regular profile.
          ProfileSelections::BuildRedirectedInIncognito()) {
#if BUILDFLAG(IS_ANDROID)
  // The sync status is necessary on Android to decide which prefs to check.
  DependsOn(SyncServiceFactory::GetInstance());
#endif
}

PasswordManagerSettingsServiceFactory::
    ~PasswordManagerSettingsServiceFactory() = default;

KeyedService* PasswordManagerSettingsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  TRACE_EVENT0("passwords", "PasswordManagerSettingsServiceCreation");
  Profile* profile = Profile::FromBrowserContext(context);
#if BUILDFLAG(IS_ANDROID)
  if (password_manager::features::UsesUnifiedPasswordManagerUi()) {
    return new PasswordManagerSettingsServiceAndroidImpl(
        profile->GetPrefs(), SyncServiceFactory::GetForProfile(profile));
  }
  // Reset the migration pref in case the client is no longer in the enabled
  // group.
  profile->GetPrefs()->SetBoolean(
      password_manager::prefs::kSettingsMigratedToUPM, false);
  return new PasswordManagerSettingsServiceImpl(profile->GetPrefs());
#else
  return new PasswordManagerSettingsServiceImpl(profile->GetPrefs());
#endif
}

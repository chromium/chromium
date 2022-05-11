// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_settings_service_factory.h"

#include "chrome/browser/password_manager/password_manager_settings_service_impl.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
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
    : BrowserContextKeyedServiceFactory(
          "PasswordManagerSettingsService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SyncServiceFactory::GetInstance());
}

PasswordManagerSettingsServiceFactory::
    ~PasswordManagerSettingsServiceFactory() = default;

KeyedService* PasswordManagerSettingsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
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

content::BrowserContext*
PasswordManagerSettingsServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // As the service is used to read prefs and that checking them depends on
  // sync status it needs to be accessed as for the regular profile.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool PasswordManagerSettingsServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

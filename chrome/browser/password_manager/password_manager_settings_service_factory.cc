// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_settings_service_factory.h"

#include "chrome/browser/password_manager/android/password_manager_settings_service_android_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

using password_manager::PasswordSettingsUpdaterAndroidBridge;

// static
PasswordManagerSettingsService*
PasswordManagerSettingsServiceFactory::GetForProfile(Profile* profile) {
  if (profile->IsOffTheRecord())
    return nullptr;

  if (!PasswordSettingsUpdaterAndroidBridge::CanCreateAccessor())
    return nullptr;

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
  return new PasswordManagerSettingsServiceAndroidImpl(
      profile->GetPrefs(), SyncServiceFactory::GetForProfile(profile));
}

content::BrowserContext*
PasswordManagerSettingsServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  if (context->IsOffTheRecord())
    return nullptr;
  return context;
}

bool PasswordManagerSettingsServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

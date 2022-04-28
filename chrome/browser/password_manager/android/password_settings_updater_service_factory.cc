// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_settings_updater_service_factory.h"

#include "chrome/browser/password_manager/android/password_settings_updater_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

using password_manager::PasswordSettingsUpdaterAndroidBridge;

// static
PasswordSettingsUpdaterService*
PasswordSettingsUpdaterServiceFactory::GetForProfile(Profile* profile) {
  if (profile->IsOffTheRecord())
    return nullptr;

  if (!PasswordSettingsUpdaterAndroidBridge::CanCreateAccessor())
    return nullptr;

  return static_cast<PasswordSettingsUpdaterService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PasswordSettingsUpdaterServiceFactory*
PasswordSettingsUpdaterServiceFactory::GetInstance() {
  return base::Singleton<PasswordSettingsUpdaterServiceFactory>::get();
}

PasswordSettingsUpdaterServiceFactory::PasswordSettingsUpdaterServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PasswordSettingsUpdaterService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SyncServiceFactory::GetInstance());
}

PasswordSettingsUpdaterServiceFactory::
    ~PasswordSettingsUpdaterServiceFactory() = default;

KeyedService* PasswordSettingsUpdaterServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new PasswordSettingsUpdaterService(
      profile->GetPrefs(), SyncServiceFactory::GetForProfile(profile));
}

content::BrowserContext*
PasswordSettingsUpdaterServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  if (context->IsOffTheRecord())
    return nullptr;
  return context;
}

bool PasswordSettingsUpdaterServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

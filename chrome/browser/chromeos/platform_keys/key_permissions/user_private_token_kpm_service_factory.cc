// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/key_permissions/user_private_token_kpm_service_factory.h"

#include <memory>

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_manager_impl.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace chromeos {
namespace platform_keys {

UserPrivateTokenKeyPermissionsManagerService::
    UserPrivateTokenKeyPermissionsManagerService(Profile* profile) {
  DCHECK(profile);

  auto arc_usage_manager_delegate =
      std::make_unique<UserPrivateTokenArcKpmDelegate>(profile);

  key_permissions_manager_ = std::make_unique<KeyPermissionsManagerImpl>(
      TokenId::kUser, std::move(arc_usage_manager_delegate),
      PlatformKeysServiceFactory::GetInstance()->GetForBrowserContext(profile),
      profile->GetPrefs());
}

UserPrivateTokenKeyPermissionsManagerService::
    UserPrivateTokenKeyPermissionsManagerService() = default;

UserPrivateTokenKeyPermissionsManagerService::
    ~UserPrivateTokenKeyPermissionsManagerService() = default;

void UserPrivateTokenKeyPermissionsManagerService::Shutdown() {
  key_permissions_manager_->Shutdown();
}

KeyPermissionsManager*
UserPrivateTokenKeyPermissionsManagerService::key_permissions_manager() {
  return key_permissions_manager_.get();
}

// static
UserPrivateTokenKeyPermissionsManagerService*
UserPrivateTokenKeyPermissionsManagerServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<UserPrivateTokenKeyPermissionsManagerService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
UserPrivateTokenKeyPermissionsManagerServiceFactory*
UserPrivateTokenKeyPermissionsManagerServiceFactory::GetInstance() {
  return base::Singleton<
      UserPrivateTokenKeyPermissionsManagerServiceFactory>::get();
}

UserPrivateTokenKeyPermissionsManagerServiceFactory::
    UserPrivateTokenKeyPermissionsManagerServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "UserPrivateTokenKeyPermissionsManagerService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(PlatformKeysServiceFactory::GetInstance());
}

UserPrivateTokenKeyPermissionsManagerServiceFactory::
    ~UserPrivateTokenKeyPermissionsManagerServiceFactory() = default;

KeyedService*
UserPrivateTokenKeyPermissionsManagerServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  if (!ProfileHelper::IsRegularProfile(profile)) {
    return nullptr;
  }

  return new UserPrivateTokenKeyPermissionsManagerService(profile);
}

bool UserPrivateTokenKeyPermissionsManagerServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool UserPrivateTokenKeyPermissionsManagerServiceFactory::
    ServiceIsNULLWhileTesting() const {
  return true;
}

void UserPrivateTokenKeyPermissionsManagerServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kKeyPermissionsOneTimeMigrationDone,
                                /*default_value=*/false);
}

}  // namespace platform_keys
}  // namespace chromeos

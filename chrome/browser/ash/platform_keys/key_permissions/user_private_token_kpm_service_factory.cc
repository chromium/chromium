// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/platform_keys/key_permissions/user_private_token_kpm_service_factory.h"

#include <memory>

#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager_impl.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace ash {
namespace platform_keys {

UserPrivateTokenKeyPermissionsManagerService::
    UserPrivateTokenKeyPermissionsManagerService(Profile* profile) {
  DCHECK(profile);

  auto arc_usage_manager_delegate =
      std::make_unique<UserPrivateTokenArcKpmDelegate>(profile);

  key_permissions_manager_ = std::make_unique<KeyPermissionsManagerImpl>(
      chromeos::platform_keys::TokenId::kUser,
      std::move(arc_usage_manager_delegate),
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
  static base::NoDestructor<UserPrivateTokenKeyPermissionsManagerServiceFactory>
      instance;
  return instance.get();
}

UserPrivateTokenKeyPermissionsManagerServiceFactory::
    UserPrivateTokenKeyPermissionsManagerServiceFactory()
    : ProfileKeyedServiceFactory("UserPrivateTokenKeyPermissionsManagerService",
                                 ProfileSelections::Builder()
                                     .WithGuest(ProfileSelection::kOriginalOnly)
                                     .WithAshInternals(ProfileSelection::kNone)
                                     .Build()) {
  DependsOn(PlatformKeysServiceFactory::GetInstance());
}

UserPrivateTokenKeyPermissionsManagerServiceFactory::
    ~UserPrivateTokenKeyPermissionsManagerServiceFactory() = default;

std::unique_ptr<KeyedService>
UserPrivateTokenKeyPermissionsManagerServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  return std::make_unique<UserPrivateTokenKeyPermissionsManagerService>(
      Profile::FromBrowserContext(context));
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
}  // namespace ash

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_service_factory.h"

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_manager_impl.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_service.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_service_impl.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/user_private_token_kpm_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace chromeos {
namespace platform_keys {

// static
KeyPermissionsService* KeyPermissionsServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<KeyPermissionsService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
KeyPermissionsServiceFactory* KeyPermissionsServiceFactory::GetInstance() {
  static base::NoDestructor<KeyPermissionsServiceFactory> factory;
  return factory.get();
}

KeyPermissionsServiceFactory::KeyPermissionsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "KeyPermissionsService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(PlatformKeysServiceFactory::GetInstance());
  DependsOn(UserPrivateTokenKeyPermissionsManagerServiceFactory::GetInstance());
}

KeyedService* KeyPermissionsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile) {
    return nullptr;
  }

  return new KeyPermissionsServiceImpl(
      ProfileHelper::IsRegularProfile(profile),
      profile->GetProfilePolicyConnector()->IsManaged(),
      PlatformKeysServiceFactory::GetForBrowserContext(profile),
      KeyPermissionsManagerImpl::GetUserPrivateTokenKeyPermissionsManager(
          profile));
}

void KeyPermissionsServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // For the format of the dictionary see prefs::kPlatformKeys documentation in
  // key_permissions_service.cc
  registry->RegisterDictionaryPref(prefs::kPlatformKeys);
}

}  // namespace platform_keys
}  // namespace chromeos

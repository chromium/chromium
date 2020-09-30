// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_manager_user_service.h"

#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_manager_impl.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "extensions/browser/extension_system.h"

namespace chromeos {
namespace platform_keys {

// ==================== KeyPermissionsManagerUserService =======================

KeyPermissionsManagerUserService::KeyPermissionsManagerUserService() = default;

KeyPermissionsManagerUserService::~KeyPermissionsManagerUserService() = default;

// ================== KeyPermissionsManagerUserServiceImpl =====================

class KeyPermissionsManagerUserServiceImpl
    : public KeyPermissionsManagerUserService {
 public:
  explicit KeyPermissionsManagerUserServiceImpl(Profile* profile)
      : key_permissions_manager_(
            profile->GetProfilePolicyConnector()->IsManaged(),
            profile->GetPrefs(),
            profile->GetProfilePolicyConnector()->policy_service(),
            extensions::ExtensionSystem::Get(profile)->state_store()) {}

  ~KeyPermissionsManagerUserServiceImpl() override = default;

  KeyPermissionsManager* key_permissions_manager() override {
    return &key_permissions_manager_;
  }

 private:
  KeyPermissionsManagerImpl key_permissions_manager_;
};

// ================== KeyPermissionsManagerUserServiceFactory ==================

// static
KeyPermissionsManagerUserService*
KeyPermissionsManagerUserServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<KeyPermissionsManagerUserService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
KeyPermissionsManagerUserServiceFactory*
KeyPermissionsManagerUserServiceFactory::GetInstance() {
  static base::NoDestructor<KeyPermissionsManagerUserServiceFactory> factory;
  return factory.get();
}

KeyPermissionsManagerUserServiceFactory::
    KeyPermissionsManagerUserServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "KeyPermissionsManagerUserService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

KeyedService* KeyPermissionsManagerUserServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile) {
    return nullptr;
  }

  return new KeyPermissionsManagerUserServiceImpl(profile);
}

void KeyPermissionsManagerUserServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // For the format of the dictionary see prefs::kPlatformKeys documentation in
  // key_permissions_manager.cc
  registry->RegisterDictionaryPref(prefs::kPlatformKeys);
}

}  // namespace platform_keys
}  // namespace chromeos

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/family_link_settings_service_factory.h"

#include "chrome/browser/profiles/profile_key.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/supervised_user/core/browser/family_link_settings_service.h"

namespace supervised_user {

// static
FamilyLinkSettingsService* FamilyLinkSettingsServiceFactory::GetForKey(
    SimpleFactoryKey* key) {
  return static_cast<FamilyLinkSettingsService*>(
      GetInstance()->GetServiceForKey(key, true));
}

// static
FamilyLinkSettingsServiceFactory*
FamilyLinkSettingsServiceFactory::GetInstance() {
  static base::NoDestructor<FamilyLinkSettingsServiceFactory> instance;
  return instance.get();
}

FamilyLinkSettingsServiceFactory::FamilyLinkSettingsServiceFactory()
    : SimpleKeyedServiceFactory("FamilyLinkSettingsService",
                                SimpleDependencyManager::GetInstance()) {}

FamilyLinkSettingsServiceFactory::~FamilyLinkSettingsServiceFactory() = default;

std::unique_ptr<KeyedService>
FamilyLinkSettingsServiceFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  return std::make_unique<supervised_user::FamilyLinkSettingsService>();
}

SimpleFactoryKey* FamilyLinkSettingsServiceFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  ProfileKey* profile_key = ProfileKey::FromSimpleFactoryKey(key);
  return profile_key->GetOriginalKey();
}
}  // namespace supervised_user

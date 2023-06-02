// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"

#include "chrome/browser/profiles/profile_key.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"

// static
supervised_user::SupervisedUserSettingsService*
SupervisedUserSettingsServiceFactory::GetForKey(SimpleFactoryKey* key) {
  return static_cast<supervised_user::SupervisedUserSettingsService*>(
      GetInstance()->GetServiceForKey(key, true));
}

// static
SupervisedUserSettingsServiceFactory*
SupervisedUserSettingsServiceFactory::GetInstance() {
  static base::NoDestructor<SupervisedUserSettingsServiceFactory> instance;
  return instance.get();
}

SupervisedUserSettingsServiceFactory::SupervisedUserSettingsServiceFactory()
    : SimpleKeyedServiceFactory("SupervisedUserSettingsService",
                                SimpleDependencyManager::GetInstance()) {}

SupervisedUserSettingsServiceFactory::~SupervisedUserSettingsServiceFactory() =
    default;

std::unique_ptr<KeyedService>
SupervisedUserSettingsServiceFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  return std::make_unique<supervised_user::SupervisedUserSettingsService>();
}

SimpleFactoryKey* SupervisedUserSettingsServiceFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  ProfileKey* profile_key = ProfileKey::FromSimpleFactoryKey(key);
  return profile_key->GetOriginalKey();
}

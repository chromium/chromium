// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_content_filters_service_factory.h"


#include "chrome/browser/profiles/profile_key.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/supervised_user/core/browser/supervised_user_content_filters_service.h"

// static
supervised_user::SupervisedUserContentFiltersService*
SupervisedUserContentFiltersServiceFactory::GetForKey(SimpleFactoryKey* key) {
  return static_cast<supervised_user::SupervisedUserContentFiltersService*>(
      GetInstance()->GetServiceForKey(key, true));
}

// static
SupervisedUserContentFiltersServiceFactory*
SupervisedUserContentFiltersServiceFactory::GetInstance() {
  static base::NoDestructor<SupervisedUserContentFiltersServiceFactory> instance;
  return instance.get();
}

SupervisedUserContentFiltersServiceFactory::SupervisedUserContentFiltersServiceFactory()
    : SimpleKeyedServiceFactory("SupervisedUserContentFiltersService",
                                SimpleDependencyManager::GetInstance()) {}

SupervisedUserContentFiltersServiceFactory::~SupervisedUserContentFiltersServiceFactory() =
    default;

std::unique_ptr<KeyedService>
SupervisedUserContentFiltersServiceFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  return std::make_unique<supervised_user::SupervisedUserContentFiltersService>();
}

SimpleFactoryKey* SupervisedUserContentFiltersServiceFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  ProfileKey* profile_key = ProfileKey::FromSimpleFactoryKey(key);
  return profile_key->GetOriginalKey();
}
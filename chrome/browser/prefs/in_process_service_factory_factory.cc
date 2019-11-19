// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/in_process_service_factory_factory.h"

#include "base/memory/singleton.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "services/preferences/public/cpp/in_process_service_factory.h"
#include "services/service_manager/public/cpp/service.h"

// static
InProcessPrefServiceFactoryFactory*
InProcessPrefServiceFactoryFactory::GetInstance() {
  return base::Singleton<InProcessPrefServiceFactoryFactory>::get();
}

// static
prefs::InProcessPrefServiceFactory*
InProcessPrefServiceFactoryFactory::GetInstanceForKey(SimpleFactoryKey* key) {
  return static_cast<prefs::InProcessPrefServiceFactory*>(
      GetInstance()->GetServiceForKey(key, true));
}

InProcessPrefServiceFactoryFactory::InProcessPrefServiceFactoryFactory()
    : SimpleKeyedServiceFactory("InProcessPrefServiceFactory",
                                SimpleDependencyManager::GetInstance()) {}

InProcessPrefServiceFactoryFactory::~InProcessPrefServiceFactoryFactory() =
    default;

std::unique_ptr<KeyedService>
InProcessPrefServiceFactoryFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  return std::make_unique<prefs::InProcessPrefServiceFactory>();
}

SimpleFactoryKey* InProcessPrefServiceFactoryFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  return key;
}

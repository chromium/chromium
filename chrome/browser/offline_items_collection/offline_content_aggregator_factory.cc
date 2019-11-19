// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"

// static
OfflineContentAggregatorFactory*
OfflineContentAggregatorFactory::GetInstance() {
  return base::Singleton<OfflineContentAggregatorFactory>::get();
}

// static
offline_items_collection::OfflineContentAggregator*
OfflineContentAggregatorFactory::GetForKey(SimpleFactoryKey* key) {
  return static_cast<offline_items_collection::OfflineContentAggregator*>(
      GetInstance()->GetServiceForKey(key, true));
}

OfflineContentAggregatorFactory::OfflineContentAggregatorFactory()
    : SimpleKeyedServiceFactory(
          "offline_items_collection::OfflineContentAggregator",
          SimpleDependencyManager::GetInstance()) {}

OfflineContentAggregatorFactory::~OfflineContentAggregatorFactory() = default;

std::unique_ptr<KeyedService>
OfflineContentAggregatorFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  DCHECK(!key->IsOffTheRecord());
  return std::make_unique<offline_items_collection::OfflineContentAggregator>();
}

SimpleFactoryKey* OfflineContentAggregatorFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  ProfileKey* profile_key = ProfileKey::FromSimpleFactoryKey(key);
  return profile_key->GetOriginalKey();
}

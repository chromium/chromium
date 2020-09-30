// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/reading_list/reading_list_manager_factory.h"

#include <memory>

#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/reading_list/android/empty_reading_list_manager.h"
#include "chrome/browser/reading_list/android/reading_list_manager_impl.h"
#include "chrome/browser/ui/read_later/reading_list_model_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/keyed_service/core/simple_dependency_manager.h"

// static
ReadingListManagerFactory* ReadingListManagerFactory::GetInstance() {
  return base::Singleton<ReadingListManagerFactory>::get();
}

// static
ReadingListManager* ReadingListManagerFactory::GetForKey(
    SimpleFactoryKey* key) {
  return static_cast<ReadingListManager*>(
      GetInstance()->GetServiceForKey(key, /*create=*/true));
}

ReadingListManagerFactory::ReadingListManagerFactory()
    : SimpleKeyedServiceFactory("ReadingListManager",
                                SimpleDependencyManager::GetInstance()) {
  DependsOn(ReadingListModelFactory::GetInstance());
}

ReadingListManagerFactory::~ReadingListManagerFactory() = default;

std::unique_ptr<KeyedService>
ReadingListManagerFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  if (!base::FeatureList::IsEnabled(features::kReadLater))
    return std::make_unique<EmptyReadingListManager>();

  auto* profile = ProfileManager::GetProfileFromProfileKey(
      ProfileKey::FromSimpleFactoryKey(key));
  auto* reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(profile);
  return std::make_unique<ReadingListManagerImpl>(reading_list_model);
}

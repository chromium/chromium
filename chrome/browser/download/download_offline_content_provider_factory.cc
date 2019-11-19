// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_offline_content_provider_factory.h"

#include <string>

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_offline_content_provider.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/transition_manager/full_browser_transition_manager.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"

namespace {
void OnProfileCreated(DownloadOfflineContentProvider* provider,
                      Profile* profile) {
  provider->OnProfileCreated(profile);
}
}  // namespace

// static
DownloadOfflineContentProviderFactory*
DownloadOfflineContentProviderFactory::GetInstance() {
  return base::Singleton<DownloadOfflineContentProviderFactory>::get();
}

// static
DownloadOfflineContentProvider*
DownloadOfflineContentProviderFactory::GetForKey(SimpleFactoryKey* key) {
  return static_cast<DownloadOfflineContentProvider*>(
      GetInstance()->GetServiceForKey(key, true));
}

DownloadOfflineContentProviderFactory::DownloadOfflineContentProviderFactory()
    : SimpleKeyedServiceFactory("DownloadOfflineContentProvider",
                                SimpleDependencyManager::GetInstance()) {}

DownloadOfflineContentProviderFactory::
    ~DownloadOfflineContentProviderFactory() = default;

std::unique_ptr<KeyedService>
DownloadOfflineContentProviderFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  OfflineContentAggregator* aggregator =
      OfflineContentAggregatorFactory::GetForKey(key);
  std::string name_space = OfflineContentAggregator::CreateUniqueNameSpace(
      OfflineItemUtils::GetDownloadNamespacePrefix(key->IsOffTheRecord()),
      key->IsOffTheRecord());

  auto provider =
      std::make_unique<DownloadOfflineContentProvider>(aggregator, name_space);
  auto callback = base::BindOnce(&OnProfileCreated, provider.get());
  FullBrowserTransitionManager::Get()->RegisterCallbackOnProfileCreation(
      key, std::move(callback));
  return provider;
}

SimpleFactoryKey* DownloadOfflineContentProviderFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  return key;
}

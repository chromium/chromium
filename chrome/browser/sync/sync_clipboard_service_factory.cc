// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_clipboard_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
SyncClipboardServiceFactory* SyncClipboardServiceFactory::GetInstance() {
  static base::NoDestructor<SyncClipboardServiceFactory> instance;
  return instance.get();
}

// static
SyncClipboardService* SyncClipboardServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SyncClipboardService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

SyncClipboardServiceFactory::SyncClipboardServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SyncClipboardService",
          BrowserContextDependencyManager::GetInstance()) {}

void SyncClipboardServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SyncClipboardService::RegisterProfilePrefs(registry);
}

KeyedService* SyncClipboardServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kSyncClipboardServiceFeature)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);

  sync_preferences::PrefServiceSyncable* prefs =
      PrefServiceSyncableFromProfile(profile);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      network::SharedURLLoaderFactory::Create(
          profile->GetURLLoaderFactory()->Clone());

  SyncClipboardService* service =
      new SyncClipboardService(prefs, url_loader_factory);
  service->Start();

  return service;
}

bool SyncClipboardServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

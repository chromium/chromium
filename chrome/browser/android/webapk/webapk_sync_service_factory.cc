// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_sync_service_factory.h"

#include "chrome/browser/android/webapk/webapk_sync_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace webapk {

// static
WebApkSyncService* WebApkSyncServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<WebApkSyncService*>(
      WebApkSyncServiceFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
WebApkSyncServiceFactory* WebApkSyncServiceFactory::GetInstance() {
  static base::NoDestructor<WebApkSyncServiceFactory> instance;
  return instance.get();
}

WebApkSyncServiceFactory::WebApkSyncServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "WebApkSyncService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
}

WebApkSyncServiceFactory::~WebApkSyncServiceFactory() = default;

std::unique_ptr<KeyedService>
WebApkSyncServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<WebApkSyncService>(profile);
}

}  // namespace webapk

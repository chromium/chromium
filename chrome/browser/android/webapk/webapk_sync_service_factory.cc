// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_sync_service_factory.h"

#include "chrome/browser/android/webapk/webapk_install_service_factory.h"
#include "chrome/browser/android/webapk/webapk_restore_manager.h"
#include "chrome/browser/android/webapk/webapk_restore_web_contents_manager.h"
#include "chrome/browser/android/webapk/webapk_sync_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/base/features.h"

namespace webapk {

// static
WebApkSyncService* WebApkSyncServiceFactory::GetForProfile(Profile* profile) {
  if (base::FeatureList::IsEnabled(syncer::kWebApkBackupAndRestoreBackend)) {
    return static_cast<WebApkSyncService*>(
        WebApkSyncServiceFactory::GetInstance()->GetServiceForBrowserContext(
            profile, /*create=*/true));
  }
  return nullptr;
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
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(WebApkInstallServiceFactory::GetInstance());
}

WebApkSyncServiceFactory::~WebApkSyncServiceFactory() = default;

std::unique_ptr<KeyedService>
WebApkSyncServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  auto restore_manager = std::make_unique<WebApkRestoreManager>(
      WebApkInstallServiceFactory::GetForBrowserContext(profile),
      std::make_unique<WebApkRestoreWebContentsManager>(profile));
  return std::make_unique<WebApkSyncService>(
      DataTypeStoreServiceFactory::GetForProfile(profile),
      std::move(restore_manager));
}

}  // namespace webapk

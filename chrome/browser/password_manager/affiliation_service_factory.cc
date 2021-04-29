// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/affiliation_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/site_affiliation/affiliation_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

AffiliationServiceFactory::AffiliationServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AffiliationService",
          BrowserContextDependencyManager::GetInstance()) {}

AffiliationServiceFactory::~AffiliationServiceFactory() = default;

AffiliationServiceFactory* AffiliationServiceFactory::GetInstance() {
  static base::NoDestructor<AffiliationServiceFactory> instance;
  return instance.get();
}

password_manager::AffiliationService* AffiliationServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<password_manager::AffiliationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

KeyedService* AffiliationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  network::SharedURLLoaderFactory* url_loader_factory =
      context->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess()
          .get();
  return new password_manager::AffiliationServiceImpl(sync_service,
                                                      url_loader_factory);
}

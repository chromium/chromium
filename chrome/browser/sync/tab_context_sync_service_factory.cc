// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/tab_context_sync_service_factory.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/sync_tab_context/http_rpc_based_ephemeral_key_fetcher.h"
#include "components/sync_tab_context/http_rpc_constants.h"
#include "components/sync_tab_context/tab_context_sync_service_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace {

std::unique_ptr<KeyedService> BuildServiceInstance(
    content::BrowserContext* context) {
  Profile* const profile = Profile::FromBrowserContext(context);
  syncer::DataTypeStoreService* const store_service =
      DataTypeStoreServiceFactory::GetForProfile(profile);
  signin::IdentityManager* const identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!store_service || !identity_manager) {
    return nullptr;
  }

  // `base::Unretained(profile)` is safe because `Profile` outlives all
  // `KeyedService` instances associated with it.
  auto ephemeral_key_fetcher =
      std::make_unique<sync_tab_context::HttpRpcBasedEphemeralKeyFetcher>(
          identity_manager,
          base::BindRepeating(&Profile::GetURLLoaderFactory,
                              base::Unretained(profile)),
          sync_tab_context::GetEphemeralKeyServerUrl());

  return std::make_unique<sync_tab_context::TabContextSyncServiceImpl>(
      store_service->GetStoreFactory(), std::move(ephemeral_key_fetcher),
      base::BindRepeating(&syncer::ReportUnrecoverableError,
                          chrome::GetChannel()));
}

}  // namespace

// static
BrowserContextKeyedServiceFactory::TestingFactory
TabContextSyncServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildServiceInstance);
}

// static
sync_tab_context::TabContextSyncService*
TabContextSyncServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<sync_tab_context::TabContextSyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
TabContextSyncServiceFactory* TabContextSyncServiceFactory::GetInstance() {
  static base::NoDestructor<TabContextSyncServiceFactory> instance;
  return instance.get();
}

TabContextSyncServiceFactory::TabContextSyncServiceFactory()
    : ProfileKeyedServiceFactory(
          "TabContextSyncService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

TabContextSyncServiceFactory::~TabContextSyncServiceFactory() = default;

std::unique_ptr<KeyedService>
TabContextSyncServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildServiceInstance(context);
}

bool TabContextSyncServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

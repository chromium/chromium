// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/tab_context_sync_service_factory.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/sync_tab_context/tab_context_sync_service_impl.h"

namespace {

std::unique_ptr<KeyedService> BuildServiceInstance(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<sync_tab_context::TabContextSyncServiceImpl>(
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory(),
      /*ephemeral_key_fetcher=*/nullptr,
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

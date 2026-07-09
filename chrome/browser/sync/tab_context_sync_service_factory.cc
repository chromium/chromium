// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/tab_context_sync_service_factory.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync_tab_context/tab_context_sync_service_impl.h"

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
              .Build()) {}

TabContextSyncServiceFactory::~TabContextSyncServiceFactory() = default;

std::unique_ptr<KeyedService>
TabContextSyncServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto change_processor =
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::ENCRYPTED_TAB_CONTEXT_CONTAINER,
          /*dump_stack=*/base::DoNothing());
  return std::make_unique<sync_tab_context::TabContextSyncServiceImpl>(
      std::move(change_processor));
}

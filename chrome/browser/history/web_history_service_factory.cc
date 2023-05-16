// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/web_history_service_factory.h"

#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace {
// Returns true if the user is signed in and full history sync is enabled,
// and false otherwise.
bool IsHistorySyncEnabled(Profile* profile) {
  syncer::SyncService* sync = SyncServiceFactory::GetForProfile(profile);
  return sync && sync->IsSyncFeatureActive() && !sync->IsLocalSyncEnabled() &&
         sync->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES);
}

}  // namespace

// static
WebHistoryServiceFactory* WebHistoryServiceFactory::GetInstance() {
  return base::Singleton<WebHistoryServiceFactory>::get();
}

// static
history::WebHistoryService* WebHistoryServiceFactory::GetForProfile(
      Profile* profile) {
  if (!IsHistorySyncEnabled(profile))
    return nullptr;

  return static_cast<history::WebHistoryService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

KeyedService* WebHistoryServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  // Ensure that the service is not instantiated or used if the user is not
  // signed into sync, or if web history is not enabled.
  if (!IsHistorySyncEnabled(profile))
    return nullptr;

  return new history::WebHistoryService(
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}

WebHistoryServiceFactory::WebHistoryServiceFactory()
    : ProfileKeyedServiceFactory(
          "WebHistoryServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

WebHistoryServiceFactory::~WebHistoryServiceFactory() {
}

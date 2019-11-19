// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_history_observer_service.h"

#include "chrome/browser/browsing_data/navigation_entry_remover.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/common/buildflags.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_service_factory.h"
#endif

BrowsingDataHistoryObserverService::BrowsingDataHistoryObserverService(
    Profile* profile)
    : profile_(profile) {
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  if (history_service)
    history_observer_.Add(history_service);
}

BrowsingDataHistoryObserverService::~BrowsingDataHistoryObserverService() {}

void BrowsingDataHistoryObserverService::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (!deletion_info.is_from_expiration())
    browsing_data::RemoveNavigationEntries(profile_, deletion_info);
}

// static
BrowsingDataHistoryObserverService::Factory*
BrowsingDataHistoryObserverService::Factory::GetInstance() {
  return base::Singleton<BrowsingDataHistoryObserverService::Factory>::get();
}

BrowsingDataHistoryObserverService::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
          "BrowsingDataHistoryObserverService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(TabRestoreServiceFactory::GetInstance());
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  DependsOn(SessionServiceFactory::GetInstance());
#endif
}

KeyedService*
BrowsingDataHistoryObserverService::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (profile->IsOffTheRecord() || profile->IsGuestSession())
    return nullptr;
  return new BrowsingDataHistoryObserverService(profile);
}

bool BrowsingDataHistoryObserverService::Factory::
    ServiceIsCreatedWithBrowserContext() const {
  // Create this service at startup to receive all deletion events.
  return true;
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/history_ui_favicon_request_handler_factory.h"

#include "base/memory/singleton.h"
#include "base/task/post_task.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "components/favicon/core/history_ui_favicon_request_handler_impl.h"
#include "components/favicon_base/favicon_types.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/browser/browser_context.h"

namespace {

favicon_base::FaviconRawBitmapResult GetSyncedFaviconForPageUrl(
    sync_sessions::SessionSyncService* session_sync_service,
    const GURL& page_url) {
  sync_sessions::OpenTabsUIDelegate* open_tabs =
      session_sync_service->GetOpenTabsUIDelegate();
  return open_tabs ? open_tabs->GetSyncedFaviconForPageURL(page_url)
                   : favicon_base::FaviconRawBitmapResult();
}

bool CanSendHistoryData(syncer::SyncService* sync_service) {
  return syncer::GetUploadToGoogleState(sync_service,
                                        syncer::ModelType::SESSIONS) ==
         syncer::UploadState::ACTIVE;
}

}  // namespace

// static
favicon::HistoryUiFaviconRequestHandler*
HistoryUiFaviconRequestHandlerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<favicon::HistoryUiFaviconRequestHandler*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
HistoryUiFaviconRequestHandlerFactory*
HistoryUiFaviconRequestHandlerFactory::GetInstance() {
  return base::Singleton<HistoryUiFaviconRequestHandlerFactory>::get();
}

HistoryUiFaviconRequestHandlerFactory::HistoryUiFaviconRequestHandlerFactory()
    : BrowserContextKeyedServiceFactory(
          "HistoryUiFaviconRequestHandler",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(FaviconServiceFactory::GetInstance());
  DependsOn(LargeIconServiceFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
}

HistoryUiFaviconRequestHandlerFactory::
    ~HistoryUiFaviconRequestHandlerFactory() {}

content::BrowserContext*
HistoryUiFaviconRequestHandlerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* HistoryUiFaviconRequestHandlerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new favicon::HistoryUiFaviconRequestHandlerImpl(
      base::BindRepeating(&GetSyncedFaviconForPageUrl,
                          SessionSyncServiceFactory::GetForProfile(profile)),
      base::BindRepeating(&CanSendHistoryData,
                          ProfileSyncServiceFactory::GetForProfile(profile)),
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      LargeIconServiceFactory::GetForBrowserContext(context));
}

bool HistoryUiFaviconRequestHandlerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

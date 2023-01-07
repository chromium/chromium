// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/history_ui_favicon_request_handler_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/favicon/core/history_ui_favicon_request_handler_impl.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_utils.h"
#include "content/public/browser/browser_context.h"

namespace {

bool CanSendHistoryData(syncer::SyncService* sync_service) {
  // SESSIONS and HISTORY both contain history-like data, so it's sufficient if
  // either of them is being uploaded.
  return syncer::GetUploadToGoogleState(sync_service,
                                        syncer::ModelType::SESSIONS) ==
             syncer::UploadState::ACTIVE ||
         syncer::GetUploadToGoogleState(sync_service,
                                        syncer::ModelType::HISTORY) ==
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
    : ProfileKeyedServiceFactory(
          "HistoryUiFaviconRequestHandler",
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(FaviconServiceFactory::GetInstance());
  DependsOn(LargeIconServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

HistoryUiFaviconRequestHandlerFactory::
    ~HistoryUiFaviconRequestHandlerFactory() {}

KeyedService* HistoryUiFaviconRequestHandlerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new favicon::HistoryUiFaviconRequestHandlerImpl(
      base::BindRepeating(&CanSendHistoryData,
                          SyncServiceFactory::GetForProfile(profile)),
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      LargeIconServiceFactory::GetForBrowserContext(context));
}

bool HistoryUiFaviconRequestHandlerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

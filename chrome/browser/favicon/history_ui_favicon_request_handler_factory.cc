// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/history_ui_favicon_request_handler_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/favicon/core/history_ui_favicon_request_handler_impl.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"
#include "content/public/browser/browser_context.h"

namespace {

bool CanSendHistoryData(syncer::SyncService* sync_service) {
  // SESSIONS and HISTORY both contain history-like data, so it's sufficient if
  // either of them is being uploaded.
  return syncer::GetUploadToGoogleState(sync_service,
                                        syncer::DataType::SESSIONS) ==
             syncer::UploadState::ACTIVE ||
         syncer::GetUploadToGoogleState(sync_service,
                                        syncer::DataType::HISTORY) ==
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
  static base::NoDestructor<HistoryUiFaviconRequestHandlerFactory> instance;
  return instance.get();
}

HistoryUiFaviconRequestHandlerFactory::HistoryUiFaviconRequestHandlerFactory()
    : ProfileKeyedServiceFactory(
          "HistoryUiFaviconRequestHandler",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(FaviconServiceFactory::GetInstance());
  DependsOn(LargeIconServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

HistoryUiFaviconRequestHandlerFactory::
    ~HistoryUiFaviconRequestHandlerFactory() = default;

std::unique_ptr<KeyedService>
HistoryUiFaviconRequestHandlerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<favicon::HistoryUiFaviconRequestHandlerImpl>(
      base::BindRepeating(&CanSendHistoryData,
                          SyncServiceFactory::GetForProfile(profile)),
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      LargeIconServiceFactory::GetForBrowserContext(context));
}

bool HistoryUiFaviconRequestHandlerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"

namespace sync_sessions {

// static
SyncSessionsWebContentsRouter*
SyncSessionsWebContentsRouterFactory::GetForProfile(Profile* profile) {
  return static_cast<SyncSessionsWebContentsRouter*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SyncSessionsWebContentsRouterFactory*
SyncSessionsWebContentsRouterFactory::GetInstance() {
  static base::NoDestructor<SyncSessionsWebContentsRouterFactory> instance;
  return instance.get();
}

SyncSessionsWebContentsRouterFactory::SyncSessionsWebContentsRouterFactory()
    : ProfileKeyedServiceFactory(
          "SyncSessionsWebContentsRouter",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

SyncSessionsWebContentsRouterFactory::~SyncSessionsWebContentsRouterFactory() =
    default;

std::unique_ptr<KeyedService>
SyncSessionsWebContentsRouterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<SyncSessionsWebContentsRouter>(
      static_cast<Profile*>(context));
}

}  // namespace sync_sessions

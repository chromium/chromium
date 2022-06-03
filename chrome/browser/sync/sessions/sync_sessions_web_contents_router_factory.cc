// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"

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
  return base::Singleton<SyncSessionsWebContentsRouterFactory>::get();
}

SyncSessionsWebContentsRouterFactory::SyncSessionsWebContentsRouterFactory()
    : BrowserContextKeyedServiceFactory(
          "SyncSessionsWebContentsRouter",
          BrowserContextDependencyManager::GetInstance()) {}

SyncSessionsWebContentsRouterFactory::~SyncSessionsWebContentsRouterFactory() {}

KeyedService* SyncSessionsWebContentsRouterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new SyncSessionsWebContentsRouter(static_cast<Profile*>(context));
}

}  // namespace sync_sessions

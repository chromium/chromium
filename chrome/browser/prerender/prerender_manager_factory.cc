// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_manager_factory.h"

#include "base/trace_event/trace_event.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/prerender/chrome_prerender_manager_delegate.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prerender/browser/prerender_manager.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#endif

namespace prerender {

// static
PrerenderManager* PrerenderManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  TRACE_EVENT0("browser", "PrerenderManagerFactory::GetForProfile")
  return static_cast<PrerenderManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PrerenderManagerFactory* PrerenderManagerFactory::GetInstance() {
  return base::Singleton<PrerenderManagerFactory>::get();
}

PrerenderManagerFactory::PrerenderManagerFactory()
    : BrowserContextKeyedServiceFactory(
        "PrerenderManager",
        BrowserContextDependencyManager::GetInstance()) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
#endif
  // PrerenderLocalPredictor observers the history visit DB.
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(predictors::PredictorDatabaseFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
}

PrerenderManagerFactory::~PrerenderManagerFactory() {
}

KeyedService* PrerenderManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new PrerenderManager(
      Profile::FromBrowserContext(browser_context),
      std::make_unique<ChromePrerenderManagerDelegate>(
          Profile::FromBrowserContext(browser_context)));
}

content::BrowserContext* PrerenderManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace prerender

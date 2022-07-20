// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"

#include "base/trace_event/trace_event.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_manager_delegate.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#endif

namespace prerender {

// static
NoStatePrefetchManager* NoStatePrefetchManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  TRACE_EVENT0("browser", "NoStatePrefetchManagerFactory::GetForProfile");
  return static_cast<NoStatePrefetchManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
NoStatePrefetchManagerFactory* NoStatePrefetchManagerFactory::GetInstance() {
  return base::Singleton<NoStatePrefetchManagerFactory>::get();
}

NoStatePrefetchManagerFactory::NoStatePrefetchManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "NoStatePrefetchManager",
          BrowserContextDependencyManager::GetInstance()) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
#endif
  // PrerenderLocalPredictor observers the history visit DB.
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(predictors::PredictorDatabaseFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

NoStatePrefetchManagerFactory::~NoStatePrefetchManagerFactory() {}

KeyedService* NoStatePrefetchManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new NoStatePrefetchManager(
      Profile::FromBrowserContext(browser_context),
      std::make_unique<ChromeNoStatePrefetchManagerDelegate>(
          Profile::FromBrowserContext(browser_context)));
}

content::BrowserContext* NoStatePrefetchManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace prerender

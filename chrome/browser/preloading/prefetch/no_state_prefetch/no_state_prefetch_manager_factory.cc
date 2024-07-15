// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"

#include "base/trace_event/trace_event.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_manager_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/api/declarative/rules_registry_service.h"
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
  static base::NoDestructor<NoStatePrefetchManagerFactory> instance;
  return instance.get();
}

NoStatePrefetchManagerFactory::NoStatePrefetchManagerFactory()
    : ProfileKeyedServiceFactory(
          "NoStatePrefetchManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  // NoStatePrefetchService has an indirect dependency on the
  // RulesRegistryService through extensions::TabHelper::WebContentsDestroyed.
  DependsOn(extensions::RulesRegistryService::GetFactoryInstance());
#endif
  // PrerenderLocalPredictor observers the history visit DB.
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(predictors::PredictorDatabaseFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

NoStatePrefetchManagerFactory::~NoStatePrefetchManagerFactory() = default;

std::unique_ptr<KeyedService>
NoStatePrefetchManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  return std::make_unique<NoStatePrefetchManager>(
      Profile::FromBrowserContext(browser_context),
      std::make_unique<ChromeNoStatePrefetchManagerDelegate>(
          Profile::FromBrowserContext(browser_context)));
}

}  // namespace prerender

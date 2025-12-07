// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/search_engine_preconnector_keyed_service_factory.h"

#include "base/metrics/field_trial_params.h"
#include "chrome/browser/navigation_predictor/search_engine_preconnector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "content/public/browser/browser_context.h"

namespace {

ProfileSelections GetProfileSelections() {
  // Current SearchEnginePreconnector only runs on normal mode. We will run an
  // experiment to enable the preconnector for Incognito as well.
  // Tracking bug in crbug.com/394204688.
  bool run_on_otr = SearchEnginePreconnector::ShouldBeEnabledForOffTheRecord();

  ProfileSelection profile_selection = run_on_otr
                                           ? ProfileSelection::kOwnInstance
                                           : ProfileSelection::kOriginalOnly;
  return ProfileSelections::Builder()
      .WithRegular(profile_selection)
      .WithGuest(profile_selection)
      .WithAshInternals(profile_selection)
      .Build();
}

base::LazyInstance<SearchEnginePreconnectorKeyedServiceFactory>::
    DestructorAtExit g_search_engine_preconnector_keyed_service_factory =
        LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
SearchEnginePreconnector*
SearchEnginePreconnectorKeyedServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SearchEnginePreconnector*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SearchEnginePreconnectorKeyedServiceFactory*
SearchEnginePreconnectorKeyedServiceFactory::GetInstance() {
  return g_search_engine_preconnector_keyed_service_factory.Pointer();
}

SearchEnginePreconnectorKeyedServiceFactory::
    SearchEnginePreconnectorKeyedServiceFactory()
    : ProfileKeyedServiceFactory("SearchEnginePreconnector",
                                 GetProfileSelections()) {}

SearchEnginePreconnectorKeyedServiceFactory::
    ~SearchEnginePreconnectorKeyedServiceFactory() = default;

bool SearchEnginePreconnectorKeyedServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return SearchEnginePreconnector::ShouldBeEnabledAsKeyedService();
}

std::unique_ptr<KeyedService> SearchEnginePreconnectorKeyedServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  if (!SearchEnginePreconnector::ShouldBeEnabledAsKeyedService()) {
    return nullptr;
  }

  auto search_engine_preconnector =
      std::make_unique<SearchEnginePreconnector>(context);

  // Start preconnecting to the search engine.
  if (search_engine_preconnector) {
    search_engine_preconnector->StartPreconnecting(
        /*with_startup_delay=*/true);
  }
  return search_engine_preconnector;
}

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"

#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

namespace {

base::LazyInstance<NavigationPredictorKeyedServiceFactory>::DestructorAtExit
    g_navigation_predictor_keyed_service_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
NavigationPredictorKeyedService*
NavigationPredictorKeyedServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<NavigationPredictorKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
NavigationPredictorKeyedServiceFactory*
NavigationPredictorKeyedServiceFactory::GetInstance() {
  return g_navigation_predictor_keyed_service_factory.Pointer();
}

NavigationPredictorKeyedServiceFactory::NavigationPredictorKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "NavigationPredictorKeyedService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

NavigationPredictorKeyedServiceFactory::
    ~NavigationPredictorKeyedServiceFactory() {}

std::unique_ptr<KeyedService>
  NavigationPredictorKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<NavigationPredictorKeyedService>(context);
}

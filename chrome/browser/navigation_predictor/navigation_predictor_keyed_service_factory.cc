// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"

#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
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
    : BrowserContextKeyedServiceFactory(
          "NavigationPredictorKeyedService",
          BrowserContextDependencyManager::GetInstance()) {}

NavigationPredictorKeyedServiceFactory::
    ~NavigationPredictorKeyedServiceFactory() {}

KeyedService* NavigationPredictorKeyedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new NavigationPredictorKeyedService(context);
}

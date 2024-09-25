// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/app_termination_observer_factory.h"

#include "chrome/browser/apps/platform_apps/app_termination_observer.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace chrome_apps {

// static
AppTerminationObserverFactory* AppTerminationObserverFactory::GetInstance() {
  static base::NoDestructor<AppTerminationObserverFactory> factory;
  return factory.get();
}

AppTerminationObserverFactory::AppTerminationObserverFactory()
    : ProfileKeyedServiceFactory(
          "AppTerminationObserver",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

AppTerminationObserverFactory::~AppTerminationObserverFactory() = default;

KeyedService* AppTerminationObserverFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new AppTerminationObserver(browser_context);
}

bool AppTerminationObserverFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace chrome_apps

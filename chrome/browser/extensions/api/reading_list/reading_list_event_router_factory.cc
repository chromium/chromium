// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/reading_list/reading_list_event_router_factory.h"

#include "chrome/browser/extensions/api/reading_list/reading_list_event_router.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "extensions/browser/event_router_factory.h"

namespace extensions {

// static
ReadingListEventRouter* ReadingListEventRouterFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ReadingListEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
ReadingListEventRouterFactory* ReadingListEventRouterFactory::GetInstance() {
  static base::NoDestructor<ReadingListEventRouterFactory> factory;
  return factory.get();
}

ReadingListEventRouterFactory::ReadingListEventRouterFactory()
    : ProfileKeyedServiceFactory(
          "ReadingListEventRouter",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(EventRouterFactory::GetInstance());
  DependsOn(ReadingListModelFactory::GetInstance());
}

ReadingListEventRouterFactory::~ReadingListEventRouterFactory() = default;

KeyedService* ReadingListEventRouterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ReadingListEventRouter(context);
}

bool ReadingListEventRouterFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

// Since there is a dependency on `EventRouter` that is null by default in unit
// tests, this service needs to be null as well. If we want to enable it in a
// specific test we need to override the factories for both `EventRouter` and
// this factory to enforce the service creation.
bool ReadingListEventRouterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions

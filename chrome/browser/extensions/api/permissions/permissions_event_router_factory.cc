// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/permissions/permissions_event_router_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/extensions/api/permissions/permissions_event_router.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/permissions_manager.h"

namespace extensions {

// static
PermissionsEventRouterFactory* PermissionsEventRouterFactory::GetInstance() {
  static base::NoDestructor<PermissionsEventRouterFactory> factory;
  return factory.get();
}

PermissionsEventRouterFactory::PermissionsEventRouterFactory()
    : ProfileKeyedServiceFactory(
          "PermissionsEventRouter",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(EventRouterFactory::GetInstance());
  DependsOn(PermissionsManager::GetFactory());
}

std::unique_ptr<KeyedService>
PermissionsEventRouterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<PermissionsEventRouter>(context);
}

bool PermissionsEventRouterFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions

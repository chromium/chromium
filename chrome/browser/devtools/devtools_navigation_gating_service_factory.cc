// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_navigation_gating_service_factory.h"

#include "base/types/pass_key.h"
#include "chrome/browser/devtools/devtools_navigation_gating_service.h"
#include "chrome/browser/origin_gating/origin_gating_service_factory.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/origin_gating/core/origin_gating_service.h"
#include "content/public/browser/browser_context.h"

// static
DevToolsNavigationGatingServiceFactory*
DevToolsNavigationGatingServiceFactory::GetInstance() {
  static base::NoDestructor<DevToolsNavigationGatingServiceFactory> instance;
  return instance.get();
}

// static
DevToolsNavigationGatingService*
DevToolsNavigationGatingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DevToolsNavigationGatingService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

DevToolsNavigationGatingServiceFactory::DevToolsNavigationGatingServiceFactory()
    : ProfileKeyedServiceFactory(
          "DevToolsNavigationGatingService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              .WithSystem(ProfileSelection::kNone)
              .Build()) {
  DependsOn(origin_gating::OriginGatingServiceFactory::GetInstance());
}

DevToolsNavigationGatingServiceFactory::
    ~DevToolsNavigationGatingServiceFactory() = default;

bool DevToolsNavigationGatingServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

std::unique_ptr<KeyedService>
DevToolsNavigationGatingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  origin_gating::OriginGatingService* origin_gating_service =
      origin_gating::OriginGatingServiceFactory::GetForBrowserContext(context);
  if (!origin_gating_service) {
    return nullptr;
  }
  return std::make_unique<DevToolsNavigationGatingService>(
      base::PassKey<DevToolsNavigationGatingServiceFactory>(),
      *origin_gating_service);
}

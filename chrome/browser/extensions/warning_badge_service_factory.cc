// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/warning_badge_service_factory.h"

#include "chrome/browser/extensions/warning_badge_service.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/warning_service_factory.h"

using content::BrowserContext;

namespace extensions {

// static
WarningBadgeService* WarningBadgeServiceFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<WarningBadgeService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
WarningBadgeServiceFactory* WarningBadgeServiceFactory::GetInstance() {
  static base::NoDestructor<WarningBadgeServiceFactory> instance;
  return instance.get();
}

WarningBadgeServiceFactory::WarningBadgeServiceFactory()
    : ProfileKeyedServiceFactory(
          "WarningBadgeService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(WarningServiceFactory::GetInstance());
}

WarningBadgeServiceFactory::~WarningBadgeServiceFactory() = default;

std::unique_ptr<KeyedService>
WarningBadgeServiceFactory::BuildServiceInstanceForBrowserContext(
    BrowserContext* context) const {
  return std::make_unique<WarningBadgeService>(static_cast<Profile*>(context));
}

bool WarningBadgeServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions

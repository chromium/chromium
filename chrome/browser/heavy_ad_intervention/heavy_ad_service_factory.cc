// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/heavy_ad_intervention/heavy_ad_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/heavy_ad_intervention/heavy_ad_service.h"
#include "content/public/browser/browser_context.h"

namespace {

base::LazyInstance<HeavyAdServiceFactory>::DestructorAtExit g_heavy_ad_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
heavy_ad_intervention::HeavyAdService*
HeavyAdServiceFactory::GetForBrowserContext(content::BrowserContext* context) {
  return static_cast<heavy_ad_intervention::HeavyAdService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
HeavyAdServiceFactory* HeavyAdServiceFactory::GetInstance() {
  return g_heavy_ad_factory.Pointer();
}

HeavyAdServiceFactory::HeavyAdServiceFactory()
    : ProfileKeyedServiceFactory(
          "HeavyAdService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

HeavyAdServiceFactory::~HeavyAdServiceFactory() {}

std::unique_ptr<KeyedService>
HeavyAdServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<heavy_ad_intervention::HeavyAdService>();
}

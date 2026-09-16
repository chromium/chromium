// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/origin_gating/origin_gating_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "base/types/pass_key.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/origin_gating/core/origin_gating_service.h"
#include "content/public/browser/browser_context.h"

namespace origin_gating {

// static
OriginGatingServiceFactory* OriginGatingServiceFactory::GetInstance() {
  static base::NoDestructor<OriginGatingServiceFactory> instance;
  return instance.get();
}

// static
OriginGatingService* OriginGatingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<OriginGatingService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

OriginGatingServiceFactory::OriginGatingServiceFactory()
    : ProfileKeyedServiceFactory(
          "OriginGatingService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .WithSystem(ProfileSelection::kNone)
              .Build()) {}

OriginGatingServiceFactory::~OriginGatingServiceFactory() = default;

bool OriginGatingServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

std::unique_ptr<KeyedService>
OriginGatingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<OriginGatingService>(
      base::PassKey<OriginGatingServiceFactory>());
}

}  // namespace origin_gating

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/ttc_keyed_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/features.h"
#include "chrome/browser/ttc/ttc_keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace ttc {

// static
TtcKeyedService* TtcKeyedServiceFactory::GetTtcKeyedService(
    content::BrowserContext* context) {
  return static_cast<TtcKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
TtcKeyedServiceFactory* TtcKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<TtcKeyedServiceFactory> instance{
      base::PassKey<TtcKeyedServiceFactory>()};
  return instance.get();
}

TtcKeyedServiceFactory::TtcKeyedServiceFactory(
    base::PassKey<TtcKeyedServiceFactory> pass_key)
    : ProfileKeyedServiceFactory("TtcKeyedService",
                                 ProfileSelections::BuildForRegularProfile()) {}

TtcKeyedServiceFactory::~TtcKeyedServiceFactory() = default;

bool TtcKeyedServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

std::unique_ptr<KeyedService>
TtcKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kTtc)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<TtcKeyedService>(profile);
}

}  // namespace ttc

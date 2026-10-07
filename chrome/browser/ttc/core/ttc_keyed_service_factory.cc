// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/ttc_keyed_service_factory.h"

#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
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
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  DependsOn(actor::ActorKeyedServiceFactory::GetInstance());
  DependsOn(page_content_annotations::PageContentExtractionServiceFactory::
                GetInstance());
}

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
  // TTC's tool calls run in actor tasks, so TTC isn't supported without the
  // actor service (e.g. if features::kGlicActor is disabled).
  if (!actor::ActorKeyedServiceFactory::GetActorKeyedService(context)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  if (!page_content_annotations::PageContentExtractionServiceFactory::
          GetForProfile(profile)) {
    return nullptr;
  }
  return std::make_unique<TtcKeyedService>(profile);
}

}  // namespace ttc

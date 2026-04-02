// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"

namespace contextual_cueing {

// static
ContextualCueingService* ContextualCueingServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ContextualCueingService*>(
      GetInstance()->GetServiceForBrowserContext(/*context=*/profile,
                                                 /*create=*/true));
}

// static
ContextualCueingServiceFactory* ContextualCueingServiceFactory::GetInstance() {
  static base::NoDestructor<ContextualCueingServiceFactory> instance;
  return instance.get();
}

ContextualCueingServiceFactory::ContextualCueingServiceFactory()
    : ProfileKeyedServiceFactory(
          "ContextualCueingServiceV2",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

ContextualCueingServiceFactory::~ContextualCueingServiceFactory() = default;

std::unique_ptr<KeyedService>
ContextualCueingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kContextualCueingV2)) {
    return nullptr;
  }
  return std::make_unique<ContextualCueingService>();
}

bool ContextualCueingServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return base::FeatureList::IsEnabled(kContextualCueingV2);
}

bool ContextualCueingServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace contextual_cueing

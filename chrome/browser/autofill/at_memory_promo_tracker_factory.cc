// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/at_memory_promo_tracker_factory.h"

#include "components/autofill/core/browser/at_memory_promo_tracker.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

// static
AtMemoryPromoTracker* AtMemoryPromoTrackerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AtMemoryPromoTracker*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
AtMemoryPromoTrackerFactory* AtMemoryPromoTrackerFactory::GetInstance() {
  static base::NoDestructor<AtMemoryPromoTrackerFactory> instance;
  return instance.get();
}

AtMemoryPromoTrackerFactory::AtMemoryPromoTrackerFactory()
    : ProfileKeyedServiceFactory(
          "AtMemoryPromoTracker",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .Build()) {}

AtMemoryPromoTrackerFactory::~AtMemoryPromoTrackerFactory() = default;

std::unique_ptr<KeyedService>
AtMemoryPromoTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kAutofillAtMemory)) {
    return nullptr;
  }
  return std::make_unique<AtMemoryPromoTracker>();
}

}  // namespace autofill

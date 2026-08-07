// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/entity_suppression_manager_factory.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/in_memory_entity_suppression_manager.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

// static
EntitySuppressionManager* EntitySuppressionManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<EntitySuppressionManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
EntitySuppressionManagerFactory*
EntitySuppressionManagerFactory::GetInstance() {
  static base::NoDestructor<EntitySuppressionManagerFactory> instance;
  return instance.get();
}

EntitySuppressionManagerFactory::EntitySuppressionManagerFactory()
    : ProfileKeyedServiceFactory(
          "EntitySuppressionManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

EntitySuppressionManagerFactory::~EntitySuppressionManagerFactory() = default;

std::unique_ptr<KeyedService>
EntitySuppressionManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAmbientAutofillSuppression)) {
    return nullptr;
  }
  return std::make_unique<InMemoryEntitySuppressionManager>();
}

}  // namespace autofill

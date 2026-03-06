// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/accessibility_query_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/accessibility_annotator/core/accessibility_query_service.h"
#include "components/autofill/core/browser/at_memory/autofill_data_provider_impl.h"
#include "components/autofill/core/common/autofill_features.h"

namespace accessibility_annotator {

// static
AccessibilityQueryServiceFactory*
AccessibilityQueryServiceFactory::GetInstance() {
  static base::NoDestructor<AccessibilityQueryServiceFactory> instance;
  return instance.get();
}

// static
AccessibilityQueryService* AccessibilityQueryServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AccessibilityQueryService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

AccessibilityQueryServiceFactory::AccessibilityQueryServiceFactory()
    : ProfileKeyedServiceFactory("AccessibilityQueryService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(autofill::PersonalDataManagerFactory::GetInstance());
  DependsOn(autofill::AutofillEntityDataManagerFactory::GetInstance());
}

AccessibilityQueryServiceFactory::~AccessibilityQueryServiceFactory() = default;

std::unique_ptr<KeyedService>
AccessibilityQueryServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(autofill::features::kAutofillAtMemory)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  auto data_provider = std::make_unique<autofill::AutofillDataProviderImpl>(
      autofill::PersonalDataManagerFactory::GetForBrowserContext(context),
      autofill::AutofillEntityDataManagerFactory::GetForProfile(profile));

  return std::make_unique<AccessibilityQueryService>(std::move(data_provider));
}

bool AccessibilityQueryServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return false;
}

}  // namespace accessibility_annotator

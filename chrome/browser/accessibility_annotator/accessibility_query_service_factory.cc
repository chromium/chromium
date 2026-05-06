// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/accessibility_query_service_factory.h"

#include <memory>
#include <vector>

#include "base/no_destructor.h"
#include "chrome/browser/accessibility_annotator/accessibility_annotator_backend_factory.h"
#include "chrome/browser/accessibility_annotator/accessibility_query_service_delegate_impl.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/accessibility_query_service.h"
#include "components/accessibility_annotator/core/annotation_reducer/one_p_resolver_impl.h"
#include "components/accessibility_annotator/core/annotation_reducer/sync_bridge_data_provider.h"
#include "components/autofill/core/browser/at_memory/autofill_data_provider_impl.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// static
AccessibilityQueryServiceFactory*
AccessibilityQueryServiceFactory::GetInstance() {
  static base::NoDestructor<AccessibilityQueryServiceFactory> instance;
  return instance.get();
}

// static
accessibility_annotator::AccessibilityQueryService*
AccessibilityQueryServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<accessibility_annotator::AccessibilityQueryService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

AccessibilityQueryServiceFactory::AccessibilityQueryServiceFactory()
    : ProfileKeyedServiceFactory("AccessibilityQueryService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(autofill::PersonalDataManagerFactory::GetInstance());
  DependsOn(autofill::AutofillEntityDataManagerFactory::GetInstance());
  DependsOn(AccessibilityAnnotatorBackendFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

AccessibilityQueryServiceFactory::~AccessibilityQueryServiceFactory() = default;

std::unique_ptr<KeyedService>
AccessibilityQueryServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(autofill::features::kAutofillAtMemory)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  std::vector<std::unique_ptr<accessibility_annotator::MemoryDataProvider>>
      data_providers;

  data_providers.push_back(std::make_unique<autofill::AutofillDataProviderImpl>(
      autofill::PersonalDataManagerFactory::GetForBrowserContext(context),
      autofill::AutofillEntityDataManagerFactory::GetForProfile(profile)));

  if (auto* backend =
          AccessibilityAnnotatorBackendFactory::GetForProfile(profile)) {
    data_providers.push_back(
        std::make_unique<accessibility_annotator::SyncBridgeDataProvider>(
            *backend));
  }

  auto* optimization_guide_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);

  std::unique_ptr<accessibility_annotator::OnePResolver> one_p_resolver;
  if (base::FeatureList::IsEnabled(
          accessibility_annotator::features::
              kAccessibilityAnnotationReducerOnePResolver)) {
    one_p_resolver =
        std::make_unique<accessibility_annotator::OnePResolverImpl>(
            profile->GetDefaultStoragePartition()
                ->GetURLLoaderFactoryForBrowserProcess(),
            IdentityManagerFactory::GetForProfile(profile),
            optimization_guide_service);
  }

  return std::make_unique<accessibility_annotator::AccessibilityQueryService>(
      std::make_unique<
          accessibility_annotator::AccessibilityQueryServiceDelegateImpl>(
          profile),
      std::move(data_providers), std::move(one_p_resolver),
      optimization_guide_service);
}

bool AccessibilityQueryServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return false;
}

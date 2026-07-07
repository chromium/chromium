// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/at_memory_query_service_factory.h"

#include <memory>
#include <vector>

#include "base/no_destructor.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/personal_context/personal_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/autofill/core/browser/at_memory/at_memory_enablement_utils.h"
#include "components/autofill/core/browser/at_memory/autofill_data_provider.h"
#include "components/autofill/core/browser/integrators/at_memory/at_memory_query_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// static
AtMemoryQueryServiceFactory* AtMemoryQueryServiceFactory::GetInstance() {
  static base::NoDestructor<AtMemoryQueryServiceFactory> instance;
  return instance.get();
}

// static
autofill::AtMemoryQueryService* AtMemoryQueryServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<autofill::AtMemoryQueryService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

AtMemoryQueryServiceFactory::AtMemoryQueryServiceFactory()
    : ProfileKeyedServiceFactory("AtMemoryQueryService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(autofill::PersonalDataManagerFactory::GetInstance());
  DependsOn(autofill::AutofillEntityDataManagerFactory::GetInstance());
  DependsOn(GoogleGroupsManagerFactory::GetInstance());
  DependsOn(PersonalContextServiceFactory::GetInstance());
}

AtMemoryQueryServiceFactory::~AtMemoryQueryServiceFactory() = default;

std::unique_ptr<KeyedService>
AtMemoryQueryServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!autofill::IsAtMemoryFeatureEnabled(
          GoogleGroupsManagerFactory::GetForBrowserContext(context))) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  auto data_provider = std::make_unique<autofill::AutofillDataProvider>(
      autofill::PersonalDataManagerFactory::GetForBrowserContext(context),
      autofill::AutofillEntityDataManagerFactory::GetForProfile(profile));

  personal_context::PersonalContextService* personal_context_service =
      PersonalContextServiceFactory::GetForProfile(profile);

  return std::make_unique<autofill::AtMemoryQueryService>(
      std::move(data_provider), personal_context_service,
      g_browser_process->GetApplicationLocale());
}

bool AtMemoryQueryServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return false;
}

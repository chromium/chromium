// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/autofill/autofill_accessibility_annotator_data_adapter_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/strike_database/strike_database_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/strike_database/strike_database.h"
#include "components/strike_database/strike_database_base.h"
#include "components/variations/service/variations_service.h"

namespace autofill {

namespace {

// Return the latest country code from the chrome variation service.
// If the variation service is not available, an empty string is returned.
std::string GetCountryCodeFromVariations() {
  variations::VariationsService* variation_service =
      g_browser_process->variations_service();
  return variation_service
             ? base::ToUpperASCII(variation_service->GetLatestCountry())
             : std::string();
}
}  // namespace

// static
EntityDataManager* AutofillEntityDataManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<EntityDataManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AutofillEntityDataManagerFactory*
AutofillEntityDataManagerFactory::GetInstance() {
  static base::NoDestructor<AutofillEntityDataManagerFactory> instance;
  return instance.get();
}

AutofillEntityDataManagerFactory::AutofillEntityDataManagerFactory()
    : ProfileKeyedServiceFactory(
          "AutofillEntityDataManager",
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(WebDataServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(StrikeDatabaseFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(AutofillAccessibilityAnnotatorDataAdapterFactory::GetInstance());
}

AutofillEntityDataManagerFactory::~AutofillEntityDataManagerFactory() = default;

std::unique_ptr<KeyedService>
AutofillEntityDataManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // LINT.IfChange(AutofillAiCreateDataManager)
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAiCreateEntityDataManager) &&
      !base::FeatureList::IsEnabled(features::kAutofillAiWithDataSchema)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  scoped_refptr<autofill::AutofillWebDataService> local_storage =
      WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);
  if (!local_storage) {
    // This happens in tests because
    // WebDataServiceFactory::ServiceIsNULLWhileTesting() is true.
    return nullptr;
  }
  return std::make_unique<EntityDataManager>(
      profile->GetPrefs(), IdentityManagerFactory::GetForProfile(profile),
      SyncServiceFactory::GetForProfile(profile), std::move(local_storage),
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      StrikeDatabaseFactory::GetForProfile(profile),
      AutofillAccessibilityAnnotatorDataAdapterFactory::GetForProfile(profile),
      GeoIpCountryCode(GetCountryCodeFromVariations()));
  // LINT.ThenChange(//chrome/browser/autofill/android/java/src/org/chromium/chrome/browser/autofill/autofill_ai/EntityDataManagerFactory.java:AutofillAiCreateDataManager)
}

bool AutofillEntityDataManagerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool AutofillEntityDataManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace autofill

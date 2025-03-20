// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/valuables_data_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/keyed_service/core/keyed_service.h"

namespace autofill {

// static
ValuablesDataManager* ValuablesDataManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ValuablesDataManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ValuablesDataManagerFactory* ValuablesDataManagerFactory::GetInstance() {
  static base::NoDestructor<ValuablesDataManagerFactory> instance;
  return instance.get();
}

ValuablesDataManagerFactory::ValuablesDataManagerFactory()
    : ProfileKeyedServiceFactory(
          "AutofillValuablesDataManager",
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(WebDataServiceFactory::GetInstance());
}

ValuablesDataManagerFactory::~ValuablesDataManagerFactory() = default;

std::unique_ptr<KeyedService>
ValuablesDataManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableLoyaltyCardsFilling)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  scoped_refptr<autofill::AutofillWebDataService> account_storage =
      WebDataServiceFactory::GetAutofillWebDataForAccount(
          profile, ServiceAccessType::EXPLICIT_ACCESS);
  if (!account_storage) {
    // This happens in tests because
    // WebDataServiceFactory::ServiceIsNULLWhileTesting() is true.
    return nullptr;
  }
  return std::make_unique<ValuablesDataManager>(std::move(account_storage));
}

bool ValuablesDataManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace autofill

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_offer_manager_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace autofill {

// static
AutofillOfferManager* AutofillOfferManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AutofillOfferManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
AutofillOfferManagerFactory* AutofillOfferManagerFactory::GetInstance() {
  return base::Singleton<AutofillOfferManagerFactory>::get();
}

AutofillOfferManagerFactory::AutofillOfferManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "AutofillOfferManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(PersonalDataManagerFactory::GetInstance());
}

AutofillOfferManagerFactory::~AutofillOfferManagerFactory() = default;

KeyedService* AutofillOfferManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AutofillOfferManager(
      PersonalDataManagerFactory::GetForBrowserContext(context));
}

}  // namespace autofill

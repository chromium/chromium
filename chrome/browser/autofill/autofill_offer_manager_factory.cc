// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_offer_manager_factory.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"

namespace autofill {

// static
AutofillOfferManager* AutofillOfferManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AutofillOfferManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
AutofillOfferManagerFactory* AutofillOfferManagerFactory::GetInstance() {
  static base::NoDestructor<AutofillOfferManagerFactory> instance;
  return instance.get();
}

AutofillOfferManagerFactory::AutofillOfferManagerFactory()
    : ProfileKeyedServiceFactory(
          "AutofillOfferManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(PersonalDataManagerFactory::GetInstance());
}

AutofillOfferManagerFactory::~AutofillOfferManagerFactory() = default;

KeyedService* AutofillOfferManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AutofillOfferManager(
      PersonalDataManagerFactory::GetForBrowserContext(context));
}

}  // namespace autofill

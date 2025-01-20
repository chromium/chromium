// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/iban_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/payments/iban_manager.h"

namespace autofill {

// static
IbanManager* IbanManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<IbanManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
IbanManagerFactory* IbanManagerFactory::GetInstance() {
  static base::NoDestructor<IbanManagerFactory> instance;
  return instance.get();
}

IbanManagerFactory::IbanManagerFactory()
    : ProfileKeyedServiceFactory(
          "IbanManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(PersonalDataManagerFactory::GetInstance());
}

IbanManagerFactory::~IbanManagerFactory() = default;

std::unique_ptr<KeyedService>
IbanManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  PersonalDataManager* pdm =
      PersonalDataManagerFactory::GetForBrowserContext(context);
  PaymentsDataManager* paydm = pdm ? &pdm->payments_data_manager() : nullptr;
  return std::make_unique<IbanManager>(paydm);
}

}  // namespace autofill

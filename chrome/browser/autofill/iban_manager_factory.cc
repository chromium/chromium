// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/iban_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/iban_manager.h"

namespace autofill {

// static
IBANManager* IBANManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<IBANManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
IBANManagerFactory* IBANManagerFactory::GetInstance() {
  static base::NoDestructor<IBANManagerFactory> instance;
  return instance.get();
}

IBANManagerFactory::IBANManagerFactory()
    : ProfileKeyedServiceFactory(
          "IBANManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(PersonalDataManagerFactory::GetInstance());
}

IBANManagerFactory::~IBANManagerFactory() = default;

KeyedService* IBANManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  IBANManager* service = new IBANManager(
      PersonalDataManagerFactory::GetForBrowserContext(context));
  return service;
}

}  // namespace autofill

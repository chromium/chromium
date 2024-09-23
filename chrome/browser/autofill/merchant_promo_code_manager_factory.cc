// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/merchant_promo_code_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/merchant_promo_code_manager.h"

namespace autofill {

// static
MerchantPromoCodeManager* MerchantPromoCodeManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<MerchantPromoCodeManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
MerchantPromoCodeManagerFactory*
MerchantPromoCodeManagerFactory::GetInstance() {
  static base::NoDestructor<MerchantPromoCodeManagerFactory> instance;
  return instance.get();
}

MerchantPromoCodeManagerFactory::MerchantPromoCodeManagerFactory()
    : ProfileKeyedServiceFactory(
          "MerchantPromoCodeManager",
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

MerchantPromoCodeManagerFactory::~MerchantPromoCodeManagerFactory() = default;

KeyedService* MerchantPromoCodeManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  MerchantPromoCodeManager* service = new MerchantPromoCodeManager();
  service->Init(PersonalDataManagerFactory::GetForBrowserContext(context),
                profile->IsOffTheRecord());
  return service;
}

}  // namespace autofill

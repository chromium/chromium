// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/merchant_promo_code_manager_factory.h"

#include "base/memory/singleton.h"
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
  return base::Singleton<MerchantPromoCodeManagerFactory>::get();
}

MerchantPromoCodeManagerFactory::MerchantPromoCodeManagerFactory()
    : ProfileKeyedServiceFactory(
          "MerchantPromoCodeManager",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(PersonalDataManagerFactory::GetInstance());
}

MerchantPromoCodeManagerFactory::~MerchantPromoCodeManagerFactory() = default;

KeyedService* MerchantPromoCodeManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  raw_ptr<MerchantPromoCodeManager> service = new MerchantPromoCodeManager();
  service->Init(PersonalDataManagerFactory::GetForBrowserContext(context),
                profile->IsOffTheRecord());
  return service;
}

}  // namespace autofill

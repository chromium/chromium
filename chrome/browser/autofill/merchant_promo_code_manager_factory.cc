// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/merchant_promo_code_manager_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/merchant_promo_code_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

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
    : BrowserContextKeyedServiceFactory(
          "MerchantPromoCodeManager",
          BrowserContextDependencyManager::GetInstance()) {
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

content::BrowserContext*
MerchantPromoCodeManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace autofill

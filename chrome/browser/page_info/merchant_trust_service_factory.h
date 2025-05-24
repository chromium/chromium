// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_INFO_MERCHANT_TRUST_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PAGE_INFO_MERCHANT_TRUST_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace page_info {
class MerchantTrustService;
}

// This factory helps construct and find the MerchantTrustService instance for a
// Profile.
class MerchantTrustServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static page_info::MerchantTrustService* GetForProfile(Profile* profile);
  static MerchantTrustServiceFactory* GetInstance();

  MerchantTrustServiceFactory(const MerchantTrustServiceFactory&) = delete;
  MerchantTrustServiceFactory& operator=(const MerchantTrustServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<MerchantTrustServiceFactory>;

  MerchantTrustServiceFactory();
  ~MerchantTrustServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;

  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_PAGE_INFO_MERCHANT_TRUST_SERVICE_FACTORY_H_

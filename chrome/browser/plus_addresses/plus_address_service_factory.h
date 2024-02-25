// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/plus_addresses/plus_address_service.h"

namespace content {
class BrowserContext;
}

// A standard ProfileKeyedServiceFactory implementation for (eventually)
// offering plus_addresses in autofill.
class PlusAddressServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static PlusAddressServiceFactory* GetInstance();
  static plus_addresses::PlusAddressService* GetForBrowserContext(
      content::BrowserContext* context);

  // Creates the profile selections used to control the types of profiles for
  // which `PlusAddressService`s will be created. Importantly, returns none
  // unless the feature is enabled.
  static ProfileSelections CreateProfileSelections();

 private:
  friend base::NoDestructor<PlusAddressServiceFactory>;

  PlusAddressServiceFactory();
  ~PlusAddressServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_FACTORY_H_

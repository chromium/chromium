// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CART_CART_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CART_CART_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class CartService;

// Factory to create CartService per profile. CartService is not supported on
// incognito, and the factory will return nullptr for an incognito profile.
class CartServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Acquire instance of CartServiceFactory.
  static CartServiceFactory* GetInstance();

  // Acquire CartService - there is one per profile.
  static CartService* GetForProfile(Profile* profile);

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  friend base::NoDestructor<CartServiceFactory>;

  CartServiceFactory();
  ~CartServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_CART_CART_SERVICE_FACTORY_H_

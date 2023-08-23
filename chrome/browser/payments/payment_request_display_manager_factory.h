// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_DISPLAY_MANAGER_FACTORY_H_
#define CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_DISPLAY_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace payments {

class PaymentRequestDisplayManager;

class PaymentRequestDisplayManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static PaymentRequestDisplayManagerFactory* GetInstance();
  static PaymentRequestDisplayManager* GetForBrowserContext(
      content::BrowserContext* context);

  PaymentRequestDisplayManagerFactory(
      const PaymentRequestDisplayManagerFactory&) = delete;
  PaymentRequestDisplayManagerFactory& operator=(
      const PaymentRequestDisplayManagerFactory&) = delete;

 private:
  PaymentRequestDisplayManagerFactory();
  ~PaymentRequestDisplayManagerFactory() override;
  friend base::NoDestructor<PaymentRequestDisplayManagerFactory>;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_DISPLAY_MANAGER_FACTORY_H_

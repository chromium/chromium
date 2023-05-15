// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/payment_request_display_manager_factory.h"

#include "components/payments/content/payment_request_display_manager.h"

namespace payments {

PaymentRequestDisplayManagerFactory*
PaymentRequestDisplayManagerFactory::GetInstance() {
  return base::Singleton<PaymentRequestDisplayManagerFactory>::get();
}

PaymentRequestDisplayManager*
PaymentRequestDisplayManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PaymentRequestDisplayManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

PaymentRequestDisplayManagerFactory::PaymentRequestDisplayManagerFactory()
    : ProfileKeyedServiceFactory(
          "PaymentRequestDisplayManager",
          // Returns non-NULL even for Incognito contexts so that a separate
          // instance of a service is created for the Incognito context.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {}

PaymentRequestDisplayManagerFactory::~PaymentRequestDisplayManagerFactory() {}

KeyedService* PaymentRequestDisplayManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new PaymentRequestDisplayManager();
}

}  // namespace payments

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/payment_request_display_manager_factory.h"

#include "components/payments/content/payment_request_display_manager.h"

namespace payments {

PaymentRequestDisplayManagerFactory*
PaymentRequestDisplayManagerFactory::GetInstance() {
  static base::NoDestructor<PaymentRequestDisplayManagerFactory> instance;
  return instance.get();
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
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

PaymentRequestDisplayManagerFactory::~PaymentRequestDisplayManagerFactory() =
    default;

std::unique_ptr<KeyedService>
PaymentRequestDisplayManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<PaymentRequestDisplayManager>();
}

}  // namespace payments

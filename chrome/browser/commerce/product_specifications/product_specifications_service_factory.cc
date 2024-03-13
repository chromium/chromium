// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/product_specifications/product_specifications_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"

namespace commerce {

// static
commerce::ProductSpecificationsService*
ProductSpecificationsServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  // Not available in incognito mode.
  if (context->IsOffTheRecord()) {
    return nullptr;
  }
  return static_cast<commerce::ProductSpecificationsService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ProductSpecificationsServiceFactory*
ProductSpecificationsServiceFactory::GetInstance() {
  static base::NoDestructor<ProductSpecificationsServiceFactory> instance;
  return instance.get();
}

ProductSpecificationsServiceFactory::ProductSpecificationsServiceFactory()
    : ProfileKeyedServiceFactory(
          "ProductSpecificationsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

ProductSpecificationsServiceFactory::~ProductSpecificationsServiceFactory() =
    default;

std::unique_ptr<KeyedService>
ProductSpecificationsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<commerce::ProductSpecificationsService>();
}

}  // namespace commerce

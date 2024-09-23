// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher.h"
#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
FastCheckoutCapabilitiesFetcherFactory*
FastCheckoutCapabilitiesFetcherFactory::GetInstance() {
  static base::NoDestructor<FastCheckoutCapabilitiesFetcherFactory> instance;
  return instance.get();
}

FastCheckoutCapabilitiesFetcherFactory::FastCheckoutCapabilitiesFetcherFactory()
    : ProfileKeyedServiceFactory(
          "FastCheckoutCapabilitiesFetcher",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

FastCheckoutCapabilitiesFetcherFactory::
    ~FastCheckoutCapabilitiesFetcherFactory() = default;

// static
FastCheckoutCapabilitiesFetcher*
FastCheckoutCapabilitiesFetcherFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<FastCheckoutCapabilitiesFetcher*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

std::unique_ptr<KeyedService>
FastCheckoutCapabilitiesFetcherFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  return std::make_unique<FastCheckoutCapabilitiesFetcherImpl>(
      browser_context->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}

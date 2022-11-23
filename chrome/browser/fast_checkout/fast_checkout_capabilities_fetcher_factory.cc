// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher.h"
#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

// static
FastCheckoutCapabilitiesFetcherFactory*
FastCheckoutCapabilitiesFetcherFactory::GetInstance() {
  static base::NoDestructor<FastCheckoutCapabilitiesFetcherFactory> instance;
  return instance.get();
}

FastCheckoutCapabilitiesFetcherFactory::FastCheckoutCapabilitiesFetcherFactory()
    : BrowserContextKeyedServiceFactory(
          "FastCheckoutCapabilitiesFetcher",
          BrowserContextDependencyManager::GetInstance()) {}

FastCheckoutCapabilitiesFetcherFactory::
    ~FastCheckoutCapabilitiesFetcherFactory() = default;

// static
FastCheckoutCapabilitiesFetcher*
FastCheckoutCapabilitiesFetcherFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<FastCheckoutCapabilitiesFetcher*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

KeyedService* FastCheckoutCapabilitiesFetcherFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new FastCheckoutCapabilitiesFetcherImpl();
}

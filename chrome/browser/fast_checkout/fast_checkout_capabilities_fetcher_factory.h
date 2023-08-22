// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_FACTORY_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class FastCheckoutCapabilitiesFetcher;

namespace content {
class BrowserContext;
}  // namespace content

// Factory for `FastCheckoutCapabilitiesFetcher` instances for a given
// `BrowserContext`.
class FastCheckoutCapabilitiesFetcherFactory
    : public ProfileKeyedServiceFactory {
 public:
  static FastCheckoutCapabilitiesFetcherFactory* GetInstance();

  FastCheckoutCapabilitiesFetcherFactory();
  ~FastCheckoutCapabilitiesFetcherFactory() override;

  static FastCheckoutCapabilitiesFetcher* GetForBrowserContext(
      content::BrowserContext* browser_context);

 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* browser_context) const override;
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_FACTORY_H_

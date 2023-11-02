// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_NETWORK_CONTEXT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_NETWORK_CONTEXT_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace safe_browsing {

class NetworkContextService;

// Factory for creating the KeyedService NetworkContextService. Incognito
// profiles will return the same NetworkContextService as the original profile.
// Features using this network context are expected to behave correctly while
// incognito.
class NetworkContextServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static NetworkContextServiceFactory* GetInstance();
  static NetworkContextService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<NetworkContextServiceFactory>;

  NetworkContextServiceFactory();
  ~NetworkContextServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_NETWORK_CONTEXT_SERVICE_FACTORY_H_

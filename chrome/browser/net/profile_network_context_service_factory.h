// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;
class ProfileNetworkContextService;

namespace content {
class BrowserContext;
}

class ProfileNetworkContextServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the ProfileNetworkContextService that supports NetworkContexts for
  // |browser_context|.
  static ProfileNetworkContextService* GetForContext(
      content::BrowserContext* browser_context);

  // Returns the NetworkContextServiceFactory singleton.
  static ProfileNetworkContextServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      ProfileNetworkContextServiceFactory>;

  ProfileNetworkContextServiceFactory();
  ~ProfileNetworkContextServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(ProfileNetworkContextServiceFactory);
};

#endif  // CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_FACTORY_H_

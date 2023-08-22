// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class ProfileNetworkContextService;

namespace content {
class BrowserContext;
}

class ProfileNetworkContextServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the ProfileNetworkContextService that supports NetworkContexts for
  // |browser_context|.
  static ProfileNetworkContextService* GetForContext(
      content::BrowserContext* browser_context);

  // Returns the NetworkContextServiceFactory singleton.
  static ProfileNetworkContextServiceFactory* GetInstance();

  ProfileNetworkContextServiceFactory(
      const ProfileNetworkContextServiceFactory&) = delete;
  ProfileNetworkContextServiceFactory& operator=(
      const ProfileNetworkContextServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ProfileNetworkContextServiceFactory>;

  ProfileNetworkContextServiceFactory();
  ~ProfileNetworkContextServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_FACTORY_H_

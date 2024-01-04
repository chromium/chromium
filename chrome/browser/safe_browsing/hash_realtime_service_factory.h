// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_HASH_REALTIME_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_HASH_REALTIME_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

namespace safe_browsing {

class HashRealTimeService;

// Singleton that owns HashRealTimeService objects, one for each active
// Profile. It listens to profile destroy events and destroy its associated
// service. It returns nullptr if the profile is in incognito or guest mode.
class HashRealTimeServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for the given |profile|.
  // If the service already exists, return its pointer.
  static HashRealTimeService* GetForProfile(Profile* profile);

  // Get the singleton instance.
  static HashRealTimeServiceFactory* GetInstance();

  HashRealTimeServiceFactory(const HashRealTimeServiceFactory&) = delete;
  HashRealTimeServiceFactory& operator=(const HashRealTimeServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<HashRealTimeServiceFactory>;

  HashRealTimeServiceFactory();
  ~HashRealTimeServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  static network::mojom::NetworkContext* GetNetworkContext(Profile* profile);
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_HASH_REALTIME_SERVICE_FACTORY_H_

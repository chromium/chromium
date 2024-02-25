// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_URL_LOOKUP_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_URL_LOOKUP_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {

class RealTimeUrlLookupService;

// Singleton that owns RealTimeUrlLookupService objects, one for each active
// Profile. It listens to profile destroy events and destroy its associated
// service. It returns nullptr if the profile is in the Incognito mode.
class RealTimeUrlLookupServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for the given |profile|.
  // If the service already exists, return its pointer.
  static RealTimeUrlLookupService* GetForProfile(Profile* profile);

  // Get the singleton instance.
  static RealTimeUrlLookupServiceFactory* GetInstance();

  RealTimeUrlLookupServiceFactory(const RealTimeUrlLookupServiceFactory&) =
      delete;
  RealTimeUrlLookupServiceFactory& operator=(
      const RealTimeUrlLookupServiceFactory&) = delete;

  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

 private:
  friend base::NoDestructor<RealTimeUrlLookupServiceFactory>;

  RealTimeUrlLookupServiceFactory();
  ~RealTimeUrlLookupServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory(
      content::BrowserContext* context) const;

  scoped_refptr<network::SharedURLLoaderFactory> testing_url_loader_factory_;
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_URL_LOOKUP_SERVICE_FACTORY_H_

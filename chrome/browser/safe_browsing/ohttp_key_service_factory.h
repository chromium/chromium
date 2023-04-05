// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_OHTTP_KEY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_OHTTP_KEY_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

namespace safe_browsing {

class OhttpKeyService;

// Singleton that owns OhttpKeyService objects, one for each active
// Profile. It listens to profile destroy events and destroy its associated
// service. It returns nullptr if the profile is in the Incognito mode.
// It returns a separate object if the profile is in Guest mode.
class OhttpKeyServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for the given |profile|.
  // If the service already exists, return its pointer.
  static OhttpKeyService* GetForProfile(Profile* profile);

  // Get the singleton instance.
  static OhttpKeyServiceFactory* GetInstance();

  OhttpKeyServiceFactory(const OhttpKeyServiceFactory&) = delete;
  OhttpKeyServiceFactory& operator=(const OhttpKeyServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<OhttpKeyServiceFactory>;

  OhttpKeyServiceFactory();
  ~OhttpKeyServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_OHTTP_KEY_SERVICE_FACTORY_H_

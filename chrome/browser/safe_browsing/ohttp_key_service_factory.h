// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_OHTTP_KEY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_OHTTP_KEY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
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
// service. It returns nullptr if the profile is in incognito or guest mode.
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
  friend base::NoDestructor<OhttpKeyServiceFactory>;

  OhttpKeyServiceFactory();
  ~OhttpKeyServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

  static std::optional<std::string> GetCountry();
};

// Used only for tests. By default, the OHTTP key service is null for tests,
// since when it's created it tries to fetch the OHTTP key, which can cause
// errors for unrelated tests. To allow the OHTTP key service in tests, create
// an object of this type and keep it in scope for as long as the override
// should exist. The constructor will set the override, and the destructor will
// clear it.
class OhttpKeyServiceAllowerForTesting {
 public:
  OhttpKeyServiceAllowerForTesting();
  OhttpKeyServiceAllowerForTesting(const OhttpKeyServiceAllowerForTesting&) =
      delete;
  OhttpKeyServiceAllowerForTesting& operator=(
      const OhttpKeyServiceAllowerForTesting&) = delete;
  ~OhttpKeyServiceAllowerForTesting();
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_OHTTP_KEY_SERVICE_FACTORY_H_

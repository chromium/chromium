// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_COOKIE_PROVIDER_SITE_COOKIE_PROVIDER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SITE_COOKIE_PROVIDER_SITE_COOKIE_PROVIDER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace site_cookie_provider {

class SiteCookieProviderService;

// Singleton that owns all SiteCookieProviderServices and associates them with
// Profiles.
class SiteCookieProviderServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static SiteCookieProviderService* GetForProfile(Profile* profile);
  static SiteCookieProviderServiceFactory* GetInstance();

  SiteCookieProviderServiceFactory(const SiteCookieProviderServiceFactory&) =
      delete;
  SiteCookieProviderServiceFactory& operator=(
      const SiteCookieProviderServiceFactory&) = delete;

 private:
  friend base::NoDestructor<SiteCookieProviderServiceFactory>;

  SiteCookieProviderServiceFactory();
  ~SiteCookieProviderServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace site_cookie_provider

#endif  // CHROME_BROWSER_SITE_COOKIE_PROVIDER_SITE_COOKIE_PROVIDER_SERVICE_FACTORY_H_

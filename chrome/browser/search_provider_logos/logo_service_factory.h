// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_PROVIDER_LOGOS_LOGO_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_PROVIDER_LOGOS_LOGO_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace search_provider_logos {
class LogoService;
}  // namespace search_provider_logos

// Singleton that owns all LogoServices and associates them with Profiles.
class LogoServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static search_provider_logos::LogoService* GetForProfile(Profile* profile);

  static LogoServiceFactory* GetInstance();

  LogoServiceFactory(const LogoServiceFactory&) = delete;
  LogoServiceFactory& operator=(const LogoServiceFactory&) = delete;

 private:
  friend base::NoDestructor<LogoServiceFactory>;

  LogoServiceFactory();
  ~LogoServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SEARCH_PROVIDER_LOGOS_LOGO_SERVICE_FACTORY_H_

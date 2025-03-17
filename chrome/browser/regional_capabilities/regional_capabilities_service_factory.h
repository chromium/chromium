// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_FACTORY_H_
#define CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace regional_capabilities {
class RegionalCapabilitiesService;
}

namespace content {
class BrowserContext;
}

class Profile;

namespace regional_capabilities {
// Singleton that owns all RegionalCapabilitiesService and associates them with
// Profiles.
class RegionalCapabilitiesServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static regional_capabilities::RegionalCapabilitiesService* GetForProfile(
      Profile* profile);

  static RegionalCapabilitiesServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<RegionalCapabilitiesServiceFactory>;

  RegionalCapabilitiesServiceFactory();
  ~RegionalCapabilitiesServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};
}  // namespace regional_capabilities

#endif  // CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_FACTORY_H_

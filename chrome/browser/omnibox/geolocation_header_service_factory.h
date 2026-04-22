// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OMNIBOX_GEOLOCATION_HEADER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_OMNIBOX_GEOLOCATION_HEADER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

class GeolocationHeaderService;
class Profile;

namespace content {
class BrowserContext;
}

class GeolocationHeaderServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static GeolocationHeaderService* GetForProfile(Profile* profile);
  static GeolocationHeaderServiceFactory* GetInstance();

  GeolocationHeaderServiceFactory(const GeolocationHeaderServiceFactory&) =
      delete;
  GeolocationHeaderServiceFactory& operator=(
      const GeolocationHeaderServiceFactory&) = delete;

 private:
  friend base::NoDestructor<GeolocationHeaderServiceFactory>;

  GeolocationHeaderServiceFactory();
  ~GeolocationHeaderServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_OMNIBOX_GEOLOCATION_HEADER_SERVICE_FACTORY_H_

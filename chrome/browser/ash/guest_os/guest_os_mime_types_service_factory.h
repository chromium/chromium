// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_MIME_TYPES_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_MIME_TYPES_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace guest_os {

class GuestOsMimeTypesService;

class GuestOsMimeTypesServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static GuestOsMimeTypesService* GetForProfile(Profile* profile);
  static GuestOsMimeTypesServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<GuestOsMimeTypesServiceFactory>;

  GuestOsMimeTypesServiceFactory();
  GuestOsMimeTypesServiceFactory(const GuestOsMimeTypesServiceFactory&) =
      delete;
  GuestOsMimeTypesServiceFactory& operator=(
      const GuestOsMimeTypesServiceFactory&) = delete;
  ~GuestOsMimeTypesServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_MIME_TYPES_SERVICE_FACTORY_H_

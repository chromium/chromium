// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_REGISTRY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_REGISTRY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace guest_os {

class GuestOsRegistryService;

class GuestOsRegistryServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static guest_os::GuestOsRegistryService* GetForProfile(Profile* profile);
  static GuestOsRegistryServiceFactory* GetInstance();

  GuestOsRegistryServiceFactory(const GuestOsRegistryServiceFactory&) = delete;
  GuestOsRegistryServiceFactory& operator=(
      const GuestOsRegistryServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<GuestOsRegistryServiceFactory>;

  GuestOsRegistryServiceFactory();
  ~GuestOsRegistryServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_REGISTRY_SERVICE_FACTORY_H_

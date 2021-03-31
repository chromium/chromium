// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_REGISTRY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_REGISTRY_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace guest_os {

class GuestOsRegistryService;

class GuestOsRegistryServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static guest_os::GuestOsRegistryService* GetForProfile(Profile* profile);
  static GuestOsRegistryServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<GuestOsRegistryServiceFactory>;

  GuestOsRegistryServiceFactory();
  ~GuestOsRegistryServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(GuestOsRegistryServiceFactory);
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_REGISTRY_SERVICE_FACTORY_H_

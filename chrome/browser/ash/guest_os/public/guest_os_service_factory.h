// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace guest_os {

class GuestOsService;

class GuestOsServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static guest_os::GuestOsService* GetForProfile(Profile* profile);
  static GuestOsServiceFactory* GetInstance();

  GuestOsServiceFactory(const GuestOsServiceFactory&) = delete;
  GuestOsServiceFactory& operator=(const GuestOsServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<GuestOsServiceFactory>;

  GuestOsServiceFactory();
  ~GuestOsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_SERVICE_FACTORY_H_

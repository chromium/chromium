// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_UNUSED_SITE_PERMISSIONS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PERMISSIONS_UNUSED_SITE_PERMISSIONS_SERVICE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace content {
class BrowserContext;
}

namespace permissions {
class UnusedSitePermissionsService;
}

class UnusedSitePermissionsServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static UnusedSitePermissionsServiceFactory* GetInstance();

  static permissions::UnusedSitePermissionsService* GetForProfile(
      Profile* profile);

  // Non-copyable, non-moveable.
  UnusedSitePermissionsServiceFactory(
      const UnusedSitePermissionsServiceFactory&) = delete;
  UnusedSitePermissionsServiceFactory& operator=(
      const UnusedSitePermissionsServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<
      UnusedSitePermissionsServiceFactory>;

  UnusedSitePermissionsServiceFactory();
  ~UnusedSitePermissionsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PERMISSIONS_UNUSED_SITE_PERMISSIONS_SERVICE_FACTORY_H_

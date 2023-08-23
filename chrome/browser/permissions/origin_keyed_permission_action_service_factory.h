// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_ORIGIN_KEYED_PERMISSION_ACTION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PERMISSIONS_ORIGIN_KEYED_PERMISSION_ACTION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace permissions {
class OriginKeyedPermissionActionService;
}
// Factory to create a service to keep track of permission actions of the
// current browser session for metrics evaluation.
class OriginKeyedPermissionActionServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  OriginKeyedPermissionActionServiceFactory(
      const OriginKeyedPermissionActionServiceFactory&) = delete;
  OriginKeyedPermissionActionServiceFactory& operator=(
      const OriginKeyedPermissionActionServiceFactory&) = delete;

  static permissions::OriginKeyedPermissionActionService* GetForProfile(
      Profile* profile);

  static OriginKeyedPermissionActionServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<OriginKeyedPermissionActionServiceFactory>;
  OriginKeyedPermissionActionServiceFactory();

  ~OriginKeyedPermissionActionServiceFactory() override;

  static permissions::OriginKeyedPermissionActionService* GetForBrowserContext(
      content::BrowserContext* browser_context);

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PERMISSIONS_ORIGIN_KEYED_PERMISSION_ACTION_SERVICE_FACTORY_H_

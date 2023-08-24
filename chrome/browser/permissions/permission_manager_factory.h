// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_MANAGER_FACTORY_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace permissions {
class PermissionManager;
}

class Profile;

class PermissionManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static permissions::PermissionManager* GetForProfile(Profile* profile);
  static PermissionManagerFactory* GetInstance();

  PermissionManagerFactory(const PermissionManagerFactory&) = delete;
  PermissionManagerFactory& operator=(const PermissionManagerFactory&) = delete;

 private:
  friend base::NoDestructor<PermissionManagerFactory>;

  PermissionManagerFactory();
  ~PermissionManagerFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_MANAGER_FACTORY_H_

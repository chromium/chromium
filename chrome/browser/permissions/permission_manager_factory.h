// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_MANAGER_FACTORY_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_MANAGER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class PermissionManager;
class Profile;

class PermissionManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PermissionManager* GetForProfile(Profile* profile);
  static PermissionManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<PermissionManagerFactory>;

  PermissionManagerFactory();
  ~PermissionManagerFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(PermissionManagerFactory);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_MANAGER_FACTORY_H_

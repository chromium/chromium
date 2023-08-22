// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_CHILD_USER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_CHILD_USER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {
class ChildUserService;

// Singleton that owns all ChildUserService objects and associates them with
// BrowserContexts. Listens for the BrowserContext's destruction notification
// and cleans up the associated ChildUserService.
class ChildUserServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ChildUserService* GetForBrowserContext(
      content::BrowserContext* context);

  static ChildUserServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ChildUserServiceFactory>;

  ChildUserServiceFactory();
  ChildUserServiceFactory(const ChildUserServiceFactory&) = delete;
  ChildUserServiceFactory& operator=(const ChildUserServiceFactory&) = delete;
  ~ChildUserServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_CHILD_USER_SERVICE_FACTORY_H_

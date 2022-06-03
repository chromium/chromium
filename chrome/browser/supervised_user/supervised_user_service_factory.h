// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}
class Profile;
class SupervisedUserService;

class SupervisedUserServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static SupervisedUserService* GetForProfile(Profile* profile);

  static SupervisedUserService* GetForBrowserContext(
      content::BrowserContext* context);

  static SupervisedUserService* GetForProfileIfExists(Profile* profile);

  static SupervisedUserServiceFactory* GetInstance();

  // Used to create instances for testing.
  static KeyedService* BuildInstanceFor(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<SupervisedUserServiceFactory>;

  SupervisedUserServiceFactory();
  ~SupervisedUserServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_FACTORY_H_

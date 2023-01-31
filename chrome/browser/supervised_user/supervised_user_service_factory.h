// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/supervised_user/core/common/supervised_users.h"

namespace content {
class BrowserContext;
}
class Profile;
class SupervisedUserService;

class SupervisedUserServiceFactory : public ProfileKeyedServiceFactory {
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
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_FACTORY_H_

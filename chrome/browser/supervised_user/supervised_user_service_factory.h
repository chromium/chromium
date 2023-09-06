// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/supervised_user/core/common/supervised_users.h"

namespace content {
class BrowserContext;
}
class Profile;

namespace supervised_user {
class SupervisedUserService;
}  // namespace supervised_user

// Factory creating SupervisedUserService for regular profiles.
// SupervisedUserService is not created for incognito and guest profile.
class SupervisedUserServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static supervised_user::SupervisedUserService* GetForProfile(
      Profile* profile);

  static supervised_user::SupervisedUserService* GetForBrowserContext(
      content::BrowserContext* context);

  static supervised_user::SupervisedUserService* GetForProfileIfExists(
      Profile* profile);

  static SupervisedUserServiceFactory* GetInstance();

  // Used to create instances for testing.
  static KeyedService* BuildInstanceFor(Profile* profile);

 private:
  friend base::NoDestructor<SupervisedUserServiceFactory>;

  SupervisedUserServiceFactory();
  ~SupervisedUserServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_FACTORY_H_

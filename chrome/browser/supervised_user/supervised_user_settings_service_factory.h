// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SETTINGS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SETTINGS_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"
#include "components/supervised_user/core/common/supervised_users.h"

class SimpleFactoryKey;

namespace supervised_user {
class SupervisedUserSettingsService;
}  // namespace supervised_user

class SupervisedUserSettingsServiceFactory : public SimpleKeyedServiceFactory {
 public:
  static supervised_user::SupervisedUserSettingsService* GetForKey(
      SimpleFactoryKey* key);

  static SupervisedUserSettingsServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<SupervisedUserSettingsServiceFactory>;

  SupervisedUserSettingsServiceFactory();
  ~SupervisedUserSettingsServiceFactory() override;

  // SimpleKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SETTINGS_SERVICE_FACTORY_H_

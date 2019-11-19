// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SETTINGS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SETTINGS_SERVICE_FACTORY_H_

#include <memory>

#include "base/memory/singleton.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"

class SimpleFactoryKey;
class SupervisedUserSettingsService;

class SupervisedUserSettingsServiceFactory : public SimpleKeyedServiceFactory {
 public:
  static SupervisedUserSettingsService* GetForKey(SimpleFactoryKey* key);

  static SupervisedUserSettingsServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      SupervisedUserSettingsServiceFactory>;

  SupervisedUserSettingsServiceFactory();
  ~SupervisedUserSettingsServiceFactory() override;

  // SimpleKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SETTINGS_SERVICE_FACTORY_H_

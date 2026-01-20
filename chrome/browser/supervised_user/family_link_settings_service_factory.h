// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_FAMILY_LINK_SETTINGS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SUPERVISED_USER_FAMILY_LINK_SETTINGS_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"
#include "components/supervised_user/core/common/supervised_users.h"

class SimpleFactoryKey;

namespace supervised_user {
class FamilyLinkSettingsService;

class FamilyLinkSettingsServiceFactory : public SimpleKeyedServiceFactory {
 public:
  static supervised_user::FamilyLinkSettingsService* GetForKey(
      SimpleFactoryKey* key);

  static FamilyLinkSettingsServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<FamilyLinkSettingsServiceFactory>;

  FamilyLinkSettingsServiceFactory();
  ~FamilyLinkSettingsServiceFactory() override;

  // SimpleKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;
};

}  // namespace supervised_user

#endif  // CHROME_BROWSER_SUPERVISED_USER_FAMILY_LINK_SETTINGS_SERVICE_FACTORY_H_

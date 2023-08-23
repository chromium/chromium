// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace crostini {

class AnsibleManagementService;

class AnsibleManagementServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static AnsibleManagementService* GetForProfile(Profile* profile);
  static AnsibleManagementServiceFactory* GetInstance();

  AnsibleManagementServiceFactory(const AnsibleManagementServiceFactory&) =
      delete;
  AnsibleManagementServiceFactory& operator=(
      const AnsibleManagementServiceFactory&) = delete;

  KeyedService* SetTestingFactoryAndUse(content::BrowserContext* context,
                                        TestingFactory testing_factory);

 private:
  friend class base::NoDestructor<AnsibleManagementServiceFactory>;

  AnsibleManagementServiceFactory();
  ~AnsibleManagementServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_SERVICE_FACTORY_H_

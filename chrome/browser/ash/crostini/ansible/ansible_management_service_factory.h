// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace crostini {

class AnsibleManagementService;

class AnsibleManagementServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static AnsibleManagementService* GetForProfile(Profile* profile);
  static AnsibleManagementServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<AnsibleManagementServiceFactory>;

  AnsibleManagementServiceFactory();
  ~AnsibleManagementServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(AnsibleManagementServiceFactory);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_SERVICE_FACTORY_H_

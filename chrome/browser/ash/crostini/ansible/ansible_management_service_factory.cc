// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/ansible/ansible_management_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_service.h"
#include "chrome/browser/profiles/profile.h"

namespace crostini {

// static
AnsibleManagementService* AnsibleManagementServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AnsibleManagementService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AnsibleManagementServiceFactory*
AnsibleManagementServiceFactory::GetInstance() {
  static base::NoDestructor<AnsibleManagementServiceFactory> factory;
  return factory.get();
}

AnsibleManagementServiceFactory::AnsibleManagementServiceFactory()
    : ProfileKeyedServiceFactory("AnsibleManagementService") {}

AnsibleManagementServiceFactory::~AnsibleManagementServiceFactory() = default;

// BrowserContextKeyedServiceFactory:
KeyedService* AnsibleManagementServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new AnsibleManagementService(profile);
}

}  // namespace crostini

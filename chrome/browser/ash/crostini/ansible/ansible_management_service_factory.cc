// Copyright 2019 The Chromium Authors
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
    : ProfileKeyedServiceFactory(
          "AnsibleManagementService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

AnsibleManagementServiceFactory::~AnsibleManagementServiceFactory() = default;

// BrowserContextKeyedServiceFactory:
KeyedService* AnsibleManagementServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new AnsibleManagementService(profile);
}

KeyedService* AnsibleManagementServiceFactory::SetTestingFactoryAndUse(
    content::BrowserContext* context,
    TestingFactory testing_factory) {
  KeyedService* mock_ansible_management_service =
      ProfileKeyedServiceFactory::SetTestingFactoryAndUse(
          context, std::move(testing_factory));
  return mock_ansible_management_service;
}

}  // namespace crostini

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_INVALIDATOR_FACTORY_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_INVALIDATOR_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace policy {

// Creates an instance of UserCloudPolicyInvalidator for each profile.
class UserCloudPolicyInvalidatorFactory : public ProfileKeyedServiceFactory {
 public:
  static UserCloudPolicyInvalidatorFactory* GetInstance();

  UserCloudPolicyInvalidatorFactory(const UserCloudPolicyInvalidatorFactory&) =
      delete;
  UserCloudPolicyInvalidatorFactory& operator=(
      const UserCloudPolicyInvalidatorFactory&) = delete;

 private:
  friend base::NoDestructor<UserCloudPolicyInvalidatorFactory>;

  UserCloudPolicyInvalidatorFactory();
  ~UserCloudPolicyInvalidatorFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_INVALIDATOR_FACTORY_H_

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_USER_CLOUD_POLICY_TOKEN_FORWARDER_FACTORY_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_USER_CLOUD_POLICY_TOKEN_FORWARDER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace policy {

// Creates instances of UserCloudPolicyTokenForwarder for Profiles that may need
// to fetch the policy token.
class UserCloudPolicyTokenForwarderFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns an instance of the UserCloudPolicyTokenForwarderFactory singleton.
  static UserCloudPolicyTokenForwarderFactory* GetInstance();

  UserCloudPolicyTokenForwarderFactory(
      const UserCloudPolicyTokenForwarderFactory&) = delete;
  UserCloudPolicyTokenForwarderFactory& operator=(
      const UserCloudPolicyTokenForwarderFactory&) = delete;

 private:
  friend base::NoDestructor<UserCloudPolicyTokenForwarderFactory>;

  UserCloudPolicyTokenForwarderFactory();
  ~UserCloudPolicyTokenForwarderFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_USER_CLOUD_POLICY_TOKEN_FORWARDER_FACTORY_H_

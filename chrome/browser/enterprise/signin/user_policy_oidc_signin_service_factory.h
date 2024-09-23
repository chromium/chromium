// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_USER_POLICY_OIDC_SIGNIN_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_USER_POLICY_OIDC_SIGNIN_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class Profile;

namespace policy {

class UserPolicyOidcSigninService;

// Singleton that owns all UserPolicyOidcSigninServices and creates/deletes them
// as new Profiles are created/shutdown.
class UserPolicyOidcSigninServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns an instance of the UserPolicyOidcSigninServiceFactory singleton.
  static UserPolicyOidcSigninServiceFactory* GetInstance();

  // Returns the instance of UserPolicyOidcSigninService for the passed
  // |profile|.
  static UserPolicyOidcSigninService* GetForProfile(Profile* profile);

  UserPolicyOidcSigninServiceFactory(
      const UserPolicyOidcSigninServiceFactory&) = delete;
  UserPolicyOidcSigninServiceFactory& operator=(
      const UserPolicyOidcSigninServiceFactory&) = delete;

 protected:
  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;

  // Overridden to cause this object to be created when the profile is created.
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend base::NoDestructor<UserPolicyOidcSigninServiceFactory>;

  UserPolicyOidcSigninServiceFactory();
  ~UserPolicyOidcSigninServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_USER_POLICY_OIDC_SIGNIN_SERVICE_FACTORY_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_POLICY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_POLICY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class SigninPolicyService;

// Singleton that owns all `SigninPolicyService`s and associates them with
// Profiles.
class SigninPolicyServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of `SigninPolicyService` associated with this profile
  // (creating one if none exists). Returns nullptr if this profile cannot have
  // an SigninPolicyService (for example, if `profile` is incognito).
  static SigninPolicyService* GetForProfile(Profile* profile);

  // Returns an instance of the factory singleton.
  static SigninPolicyServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<SigninPolicyServiceFactory>;

  SigninPolicyServiceFactory();
  ~SigninPolicyServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_POLICY_SERVICE_FACTORY_H_

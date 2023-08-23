// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_PROFILE_TOKEN_POLICY_WEB_SIGNIN_SERVICE_FACTORY_H_
#define CHROME_BROWSER_POLICY_CLOUD_PROFILE_TOKEN_POLICY_WEB_SIGNIN_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace policy {

class ProfileTokenPolicyWebSigninService;

// Singleton that owns all ProfileTokenPolicyWebSigninServices and
// creates/deletes them as new Profiles are created/shutdown.
class ProfileTokenPolicyWebSigninServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Returns an instance of the ProfileTokenPolicyWebSigninServiceFactory
  // singleton.
  static ProfileTokenPolicyWebSigninServiceFactory* GetInstance();

  // Returns the instance of ProfileTokenPolicyWebSigninService for the passed
  // |profile|. Used primarily for testing.
  static ProfileTokenPolicyWebSigninService* GetForProfile(Profile* profile);

  ProfileTokenPolicyWebSigninServiceFactory(
      const ProfileTokenPolicyWebSigninServiceFactory&) = delete;
  ProfileTokenPolicyWebSigninServiceFactory& operator=(
      const ProfileTokenPolicyWebSigninServiceFactory&) = delete;

 protected:
  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;

  // Overridden to cause this object to be created when the profile is created.
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend class base::NoDestructor<ProfileTokenPolicyWebSigninServiceFactory>;

  ProfileTokenPolicyWebSigninServiceFactory();
  ~ProfileTokenPolicyWebSigninServiceFactory() override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_PROFILE_TOKEN_POLICY_WEB_SIGNIN_SERVICE_FACTORY_H_

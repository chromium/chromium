// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_FACTORY_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace policy {

class DeviceManagementService;
class UserPolicySigninService;

// Singleton that owns all UserPolicySigninServices and creates/deletes them as
// new Profiles are created/shutdown.
class UserPolicySigninServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns an instance of the UserPolicySigninServiceFactory singleton.
  static UserPolicySigninServiceFactory* GetInstance();

  // Returns the instance of UserPolicySigninService for the passed |profile|.
  // Used primarily for testing.
  static UserPolicySigninService* GetForProfile(Profile* profile);

  // Allows setting a mock DeviceManagementService for tests. Does not take
  // ownership, and should be reset to NULL at the end of the test.
  // Set this before an instance is built for a Profile.
  static void SetDeviceManagementServiceForTesting(
      DeviceManagementService* device_management_service);

 protected:
  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  // Overridden to cause this object to be created when the profile is created.
  bool ServiceIsCreatedWithBrowserContext() const override;

  // Register the preferences related to cloud-based user policy.
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

 private:
  friend struct base::DefaultSingletonTraits<UserPolicySigninServiceFactory>;

  UserPolicySigninServiceFactory();
  ~UserPolicySigninServiceFactory() override;

  DISALLOW_COPY_AND_ASSIGN(UserPolicySigninServiceFactory);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_FACTORY_H_

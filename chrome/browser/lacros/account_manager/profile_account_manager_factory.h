// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_PROFILE_ACCOUNT_MANAGER_FACTORY_H_
#define CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_PROFILE_ACCOUNT_MANAGER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

#include "base/no_destructor.h"

class ProfileAccountManager;
class Profile;
class KeyedService;

namespace content {
class BrowserContext;
}

class ProfileAccountManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static ProfileAccountManagerFactory* GetInstance();
  static ProfileAccountManager* GetForProfile(Profile* profile);

  ProfileAccountManagerFactory(const ProfileAccountManagerFactory&) = delete;
  ProfileAccountManagerFactory& operator=(const ProfileAccountManagerFactory&) =
      delete;

 private:
  friend class base::NoDestructor<ProfileAccountManagerFactory>;

  ProfileAccountManagerFactory();
  ~ProfileAccountManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_PROFILE_ACCOUNT_MANAGER_FACTORY_H_

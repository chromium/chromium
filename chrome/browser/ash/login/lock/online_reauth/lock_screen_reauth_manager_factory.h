// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_LOCK_ONLINE_REAUTH_LOCK_SCREEN_REAUTH_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_LOCK_ONLINE_REAUTH_LOCK_SCREEN_REAUTH_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash {
class LockScreenReauthManager;

// Singleton that owns all LockScreenReauthManagers and associates them
// with Profiles.
class LockScreenReauthManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static LockScreenReauthManagerFactory* GetInstance();

  static LockScreenReauthManager* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<LockScreenReauthManagerFactory>;

  LockScreenReauthManagerFactory();
  ~LockScreenReauthManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_LOCK_ONLINE_REAUTH_LOCK_SCREEN_REAUTH_MANAGER_FACTORY_H_

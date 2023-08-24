// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chromeos/ash/services/auth_factor_config/chrome_browser_delegates.h"
#include "components/account_id/account_id.h"

class Profile;

namespace user_manager {
class User;
}

namespace ash {
namespace quick_unlock {

class QuickUnlockStorage;

// Singleton that owns all QuickUnlockStorage instances and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated QuickUnlockStorage.
class QuickUnlockFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the QuickUnlockStorage instance for `profile`.
  static QuickUnlockStorage* GetForProfile(Profile* profile);

  // Helper method that finds the QuickUnlockStorage instance for `user`. This
  // returns GetForProfile with the profile associated with `user`.
  static QuickUnlockStorage* GetForUser(const user_manager::User* user);

  // Helper method that returns the QuickUnlockStorage instance for
  // `account_id`. This returns GetForProfile with the profile associated with
  // `account_id`.
  static QuickUnlockStorage* GetForAccountId(const AccountId& account_id);

  static QuickUnlockFactory* GetInstance();

  // Returns a delegate to QuickUnlockStorage.
  static ash::auth::QuickUnlockStorageDelegate& GetDelegate();

  QuickUnlockFactory(const QuickUnlockFactory&) = delete;
  QuickUnlockFactory& operator=(const QuickUnlockFactory&) = delete;

 private:
  friend base::NoDestructor<QuickUnlockFactory>;

  QuickUnlockFactory();
  ~QuickUnlockFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace quick_unlock
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_FACTORY_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace user_manager {
class User;
}

namespace chromeos {
namespace quick_unlock {

class QuickUnlockStorage;

// Singleton that owns all QuickUnlockStorage instances and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated QuickUnlockStorage.
class QuickUnlockFactory : public BrowserContextKeyedServiceFactory {
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

 private:
  friend struct base::DefaultSingletonTraits<QuickUnlockFactory>;

  QuickUnlockFactory();
  ~QuickUnlockFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(QuickUnlockFactory);
};

}  // namespace quick_unlock
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_FACTORY_H_

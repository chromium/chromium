// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ACCOUNT_SWITCHING_MANAGER_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_ACCOUNT_SWITCHING_MANAGER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/signin/public/base/signin_buildflags.h"

static_assert(BUILDFLAG(ENABLE_DICE_SUPPORT));

class Profile;

namespace signin {
class AccountSwitchingManager;
}

// KeyedService factory that creates the Profile-associated
// `signin::AccountSwitchingManager`.
class AccountSwitchingManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the `AccountSwitchingManager` instance associated with `profile`
  // (creating one if it does not exist). Returns null if `profile` is null, is
  // not a regular profile, or the Desktop Smart Account Switching feature flags
  // are disabled.
  static signin::AccountSwitchingManager* GetForProfile(Profile* profile);

  // Returns an instance of the factory singleton.
  static AccountSwitchingManagerFactory* GetInstance();

  AccountSwitchingManagerFactory(const AccountSwitchingManagerFactory&) =
      delete;
  AccountSwitchingManagerFactory& operator=(
      const AccountSwitchingManagerFactory&) = delete;

 private:
  friend base::NoDestructor<AccountSwitchingManagerFactory>;

  AccountSwitchingManagerFactory();
  ~AccountSwitchingManagerFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // CHROME_BROWSER_SIGNIN_ACCOUNT_SWITCHING_MANAGER_FACTORY_H_

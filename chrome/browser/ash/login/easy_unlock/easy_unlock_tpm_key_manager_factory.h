// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_TPM_KEY_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_TPM_KEY_MANAGER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class AccountId;

namespace content {
class BrowserContext;
}

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace ash {
class EasyUnlockTpmKeyManager;

// Singleton factory that builds and owns all EasyUnlockTpmKeyManager services.
class EasyUnlockTpmKeyManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static EasyUnlockTpmKeyManagerFactory* GetInstance();

  static EasyUnlockTpmKeyManager* Get(content::BrowserContext* context);
  static EasyUnlockTpmKeyManager* GetForAccountId(const AccountId& account_id);

  EasyUnlockTpmKeyManagerFactory(const EasyUnlockTpmKeyManagerFactory&) =
      delete;
  EasyUnlockTpmKeyManagerFactory& operator=(
      const EasyUnlockTpmKeyManagerFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<EasyUnlockTpmKeyManagerFactory>;

  EasyUnlockTpmKeyManagerFactory();
  ~EasyUnlockTpmKeyManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_TPM_KEY_MANAGER_FACTORY_H_

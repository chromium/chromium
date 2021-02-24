// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_TPM_KEY_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_TPM_KEY_MANAGER_FACTORY_H_

#include <string>

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace chromeos {

class EasyUnlockTpmKeyManager;

// Singleton factory that builds and owns all EasyUnlockTpmKeyManager services.
class EasyUnlockTpmKeyManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static EasyUnlockTpmKeyManagerFactory* GetInstance();

  static EasyUnlockTpmKeyManager* Get(content::BrowserContext* context);
  static EasyUnlockTpmKeyManager* GetForUser(const std::string& user_id);

 private:
  friend struct base::DefaultSingletonTraits<EasyUnlockTpmKeyManagerFactory>;

  EasyUnlockTpmKeyManagerFactory();
  ~EasyUnlockTpmKeyManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockTpmKeyManagerFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_TPM_KEY_MANAGER_FACTORY_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ACCOUNT_SETTING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_ACCOUNT_SETTING_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace autofill {

class AccountSettingService;

// Factory responsible for creating `AccountSettingService`, which is
// responsible for managing settings synced via `syncer::ACCOUNT_SETTING`.
class AccountSettingServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static AccountSettingServiceFactory* GetInstance();
  static AccountSettingService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend base::NoDestructor<AccountSettingServiceFactory>;

  AccountSettingServiceFactory();
  ~AccountSettingServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ACCOUNT_SETTING_SERVICE_FACTORY_H_

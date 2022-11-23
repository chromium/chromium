// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_SERVICE_FACTORY_H_

#include "base/files/file_path.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace ash {

class EasyUnlockService;

// Singleton factory that builds and owns all EasyUnlockService.
class EasyUnlockServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static EasyUnlockServiceFactory* GetInstance();

  static EasyUnlockService* GetForBrowserContext(
      content::BrowserContext* browser_context);

  EasyUnlockServiceFactory(const EasyUnlockServiceFactory&) = delete;
  EasyUnlockServiceFactory& operator=(const EasyUnlockServiceFactory&) = delete;

  void set_app_path_for_testing(const base::FilePath& app_path) {
    app_path_for_testing_ = app_path;
  }

 private:
  friend struct base::DefaultSingletonTraits<EasyUnlockServiceFactory>;

  EasyUnlockServiceFactory();
  ~EasyUnlockServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

  base::FilePath app_path_for_testing_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_SERVICE_FACTORY_H_

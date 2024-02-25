// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SMART_LOCK_SMART_LOCK_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_SMART_LOCK_SMART_LOCK_SERVICE_FACTORY_H_

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace base {
template <typename T>
class NoDestructor;
}

namespace ash {

class SmartLockService;

// Singleton factory that builds and owns all SmartLockService.
class SmartLockServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static SmartLockServiceFactory* GetInstance();

  static SmartLockService* GetForBrowserContext(
      content::BrowserContext* browser_context);

  SmartLockServiceFactory(const SmartLockServiceFactory&) = delete;
  SmartLockServiceFactory& operator=(const SmartLockServiceFactory&) = delete;

  void set_app_path_for_testing(const base::FilePath& app_path) {
    app_path_for_testing_ = app_path;
  }

 private:
  friend base::NoDestructor<SmartLockServiceFactory>;

  SmartLockServiceFactory();
  ~SmartLockServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

  base::FilePath app_path_for_testing_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SMART_LOCK_SMART_LOCK_SERVICE_FACTORY_H_

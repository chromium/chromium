// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_IDLE_IDLE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_IDLE_IDLE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/enterprise/idle/idle_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace enterprise_idle {

class IdleServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static IdleService* GetForBrowserContext(content::BrowserContext* context);
  static IdleServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<IdleServiceFactory>;

  IdleServiceFactory();
  ~IdleServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace enterprise_idle

#endif  // CHROME_BROWSER_ENTERPRISE_IDLE_IDLE_SERVICE_FACTORY_H_

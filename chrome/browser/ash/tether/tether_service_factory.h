// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TETHER_TETHER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_TETHER_TETHER_SERVICE_FACTORY_H_

#include "chrome/browser/ash/tether/tether_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace base {
template <typename T>
class NoDestructor;
}

namespace ash {
namespace tether {

// Singleton factory that builds and owns all TetherServices.
class TetherServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static TetherServiceFactory* GetInstance();

  static TetherService* GetForBrowserContext(
      content::BrowserContext* browser_context);

  TetherServiceFactory(const TetherServiceFactory&) = delete;
  TetherServiceFactory& operator=(const TetherServiceFactory&) = delete;

 private:
  friend base::NoDestructor<TetherServiceFactory>;

  TetherServiceFactory();
  ~TetherServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace tether
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_TETHER_TETHER_SERVICE_FACTORY_H_

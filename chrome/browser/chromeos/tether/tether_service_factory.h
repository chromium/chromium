// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_TETHER_TETHER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_TETHER_TETHER_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/tether/tether_service.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

// Singleton factory that builds and owns all TetherServices.
class TetherServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static TetherServiceFactory* GetInstance();

  static TetherService* GetForBrowserContext(
      content::BrowserContext* browser_context);

 private:
  friend struct base::DefaultSingletonTraits<TetherServiceFactory>;

  TetherServiceFactory();
  ~TetherServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(TetherServiceFactory);
};

#endif  // CHROME_BROWSER_CHROMEOS_TETHER_TETHER_SERVICE_FACTORY_H_

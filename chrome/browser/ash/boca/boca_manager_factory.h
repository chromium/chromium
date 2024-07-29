// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_BOCA_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_BOCA_BOCA_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/ash/boca/boca_manager.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace ash {
class BocaManager;

// Singleton factory that builds and owns BocaManager.
class BocaManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static BocaManagerFactory* GetInstance();

  static BocaManager* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<BocaManagerFactory>;

  BocaManagerFactory();
  ~BocaManagerFactory() override;

  BocaManagerFactory(const BocaManagerFactory&) = delete;
  BocaManagerFactory& operator=(const BocaManagerFactory&) = delete;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BOCA_BOCA_MANAGER_FACTORY_H_

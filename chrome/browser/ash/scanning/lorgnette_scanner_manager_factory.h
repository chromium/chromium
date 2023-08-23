// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNING_LORGNETTE_SCANNER_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_SCANNING_LORGNETTE_SCANNER_MANAGER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

class LorgnetteScannerManager;

// Factory for LorgnetteScannerManager.
class LorgnetteScannerManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static LorgnetteScannerManager* GetForBrowserContext(
      content::BrowserContext* context);
  static LorgnetteScannerManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<LorgnetteScannerManagerFactory>;

  LorgnetteScannerManagerFactory();
  ~LorgnetteScannerManagerFactory() override;

  LorgnetteScannerManagerFactory(const LorgnetteScannerManagerFactory&) =
      delete;
  LorgnetteScannerManagerFactory& operator=(
      const LorgnetteScannerManagerFactory&) = delete;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCANNING_LORGNETTE_SCANNER_MANAGER_FACTORY_H_

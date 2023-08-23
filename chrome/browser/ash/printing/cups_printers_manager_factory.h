// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_CUPS_PRINTERS_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_PRINTING_CUPS_PRINTERS_MANAGER_FACTORY_H_

#include <memory>
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace base {
template <typename T>
class NoDestructor;
}

namespace ash {

class CupsPrintersManager;
class CupsPrintersManagerProxy;

class CupsPrintersManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static CupsPrintersManagerFactory* GetInstance();
  static CupsPrintersManager* GetForBrowserContext(
      content::BrowserContext* context);

  CupsPrintersManagerFactory(const CupsPrintersManagerFactory&) = delete;
  CupsPrintersManagerFactory& operator=(const CupsPrintersManagerFactory&) =
      delete;

  // Returns the CupsPrintersManagerProxy object which is always attached to the
  // primary profile.
  CupsPrintersManagerProxy* GetProxy();

 private:
  friend base::NoDestructor<CupsPrintersManagerFactory>;

  CupsPrintersManagerFactory();
  ~CupsPrintersManagerFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void BrowserContextShutdown(content::BrowserContext* context) override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

  // Proxy object always attached to the primary profile.
  std::unique_ptr<CupsPrintersManagerProxy> proxy_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_CUPS_PRINTERS_MANAGER_FACTORY_H_

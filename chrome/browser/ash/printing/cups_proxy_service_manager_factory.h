// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_CUPS_PROXY_SERVICE_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_PRINTING_CUPS_PROXY_SERVICE_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace ash {

class CupsProxyServiceManager;

class CupsProxyServiceManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static CupsProxyServiceManagerFactory* GetInstance();
  static CupsProxyServiceManager* GetForBrowserContext(
      content::BrowserContext* context);

  CupsProxyServiceManagerFactory(const CupsProxyServiceManagerFactory&) =
      delete;
  CupsProxyServiceManagerFactory& operator=(
      const CupsProxyServiceManagerFactory&) = delete;

 private:
  friend base::NoDestructor<CupsProxyServiceManagerFactory>;

  CupsProxyServiceManagerFactory();
  ~CupsProxyServiceManagerFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_CUPS_PROXY_SERVICE_MANAGER_FACTORY_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_CUPS_PROXY_SERVICE_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_PRINTING_CUPS_PROXY_SERVICE_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace ash {

class CupsProxyServiceManager;

class CupsProxyServiceManagerFactory
    : public BrowserContextKeyedServiceFactory {
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
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_CUPS_PROXY_SERVICE_MANAGER_FACTORY_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_NET_ENTERPRISE_PROXY_ERROR_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_NET_ENTERPRISE_PROXY_ERROR_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace enterprise_net {
class EnterpriseProxyErrorService;
}  // namespace enterprise_net

class Profile;

// Profile-scoped KeyedServiceFactory for the EnterpriseProxyErrorService.
class EnterpriseProxyErrorServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static enterprise_net::EnterpriseProxyErrorService* GetForProfile(
      Profile* profile);

  static EnterpriseProxyErrorServiceFactory* GetInstance();

  EnterpriseProxyErrorServiceFactory(
      const EnterpriseProxyErrorServiceFactory&) = delete;
  EnterpriseProxyErrorServiceFactory& operator=(
      const EnterpriseProxyErrorServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<EnterpriseProxyErrorServiceFactory>;

  EnterpriseProxyErrorServiceFactory();
  ~EnterpriseProxyErrorServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_ENTERPRISE_NET_ENTERPRISE_PROXY_ERROR_SERVICE_FACTORY_H_

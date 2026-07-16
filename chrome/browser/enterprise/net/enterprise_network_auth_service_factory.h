// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_NET_ENTERPRISE_NETWORK_AUTH_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_NET_ENTERPRISE_NETWORK_AUTH_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/enterprise/buildflags/buildflags.h"

namespace enterprise_net {
class EnterpriseNetworkAuthService;
}  // namespace enterprise_net

class Profile;

// Profile-scoped KeyedServiceFactory for EnterpriseNetworkAuthService.
class EnterpriseNetworkAuthServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static enterprise_net::EnterpriseNetworkAuthService* GetForProfile(
      Profile* profile);

  static EnterpriseNetworkAuthServiceFactory* GetInstance();

  EnterpriseNetworkAuthServiceFactory(
      const EnterpriseNetworkAuthServiceFactory&) = delete;
  EnterpriseNetworkAuthServiceFactory& operator=(
      const EnterpriseNetworkAuthServiceFactory&) = delete;

  bool ServiceIsNULLWhileTesting() const override;

 private:
  friend class base::NoDestructor<EnterpriseNetworkAuthServiceFactory>;

  EnterpriseNetworkAuthServiceFactory();
  ~EnterpriseNetworkAuthServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_ENTERPRISE_NET_ENTERPRISE_NETWORK_AUTH_SERVICE_FACTORY_H_

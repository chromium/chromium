// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_VPN_PROVIDER_VPN_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_VPN_PROVIDER_VPN_SERVICE_FACTORY_H_

#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service_interface.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {

class BrowserContext;

}  // namespace content

namespace base {
template <typename T>
class NoDestructor;
}

namespace chromeos {

using VpnServiceInterface = extensions::api::VpnServiceInterface;

// Factory to create VpnService.
class VpnServiceFactory : public ProfileKeyedServiceFactory {
 public:
  VpnServiceFactory(const VpnServiceFactory&) = delete;
  VpnServiceFactory& operator=(const VpnServiceFactory&) = delete;

  static VpnServiceInterface* GetForBrowserContext(
      content::BrowserContext* context);
  static VpnServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<VpnServiceFactory>;

  VpnServiceFactory();
  ~VpnServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_VPN_PROVIDER_VPN_SERVICE_FACTORY_H_

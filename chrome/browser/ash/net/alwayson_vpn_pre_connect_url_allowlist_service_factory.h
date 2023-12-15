// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_ALWAYSON_VPN_PRE_CONNECT_URL_ALLOWLIST_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_NET_ALWAYSON_VPN_PRE_CONNECT_URL_ALLOWLIST_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace ash {
class AlwaysOnVpnPreConnectUrlAllowlistService;

// Factory class which creates an instance of the
// `AlwaysOnVpnPreConnectUrlAllowlistService` service for the main profile, if
// the user is managed; otherwise it returns a nullptr.
class AlwaysOnVpnPreConnectUrlAllowlistServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static AlwaysOnVpnPreConnectUrlAllowlistServiceFactory* GetInstance();
  static AlwaysOnVpnPreConnectUrlAllowlistService* GetForProfile(
      Profile* profile);

  // BrowserContextKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

  void RecreateServiceInstanceForTesting(content::BrowserContext* context);

  void SetServiceIsNULLWhileTestingForTesting(
      bool service_is_null_while_testing);

 private:
  friend base::NoDestructor<AlwaysOnVpnPreConnectUrlAllowlistServiceFactory>;

  // BrowserContextKeyedServiceFactory implementation:
  bool ServiceIsNULLWhileTesting() const override;

  AlwaysOnVpnPreConnectUrlAllowlistServiceFactory();
  ~AlwaysOnVpnPreConnectUrlAllowlistServiceFactory() override;

  bool service_is_null_while_testing_ = true;
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_ALWAYSON_VPN_PRE_CONNECT_URL_ALLOWLIST_SERVICE_FACTORY_H_

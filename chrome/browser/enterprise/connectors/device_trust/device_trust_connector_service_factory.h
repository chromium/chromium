// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_CONNECTOR_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_CONNECTOR_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}

namespace enterprise_connectors {

class DeviceTrustConnectorService;

// Singleton factory for Profile-keyed DeviceTrustConnectorService instances.
class DeviceTrustConnectorServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static DeviceTrustConnectorServiceFactory* GetInstance();
  static DeviceTrustConnectorService* GetForProfile(Profile* profile);

 protected:
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

 private:
  friend base::NoDestructor<DeviceTrustConnectorServiceFactory>;

  friend class DeviceTrustConnectorServiceFactoryBaseTest;

  DeviceTrustConnectorServiceFactory();
  ~DeviceTrustConnectorServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_CONNECTOR_SERVICE_FACTORY_H_

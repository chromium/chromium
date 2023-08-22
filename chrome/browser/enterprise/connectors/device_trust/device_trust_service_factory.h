// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}

namespace enterprise_connectors {

class DeviceTrustService;

// Singleton that owns a single DeviceTrustService instance.
class DeviceTrustServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static DeviceTrustServiceFactory* GetInstance();
  static DeviceTrustService* GetForProfile(Profile* profile);

 protected:
  bool ServiceIsNULLWhileTesting() const override;

 private:
  friend base::NoDestructor<DeviceTrustServiceFactory>;

  DeviceTrustServiceFactory();
  ~DeviceTrustServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_FACTORY_H_

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace enterprise_connectors {

class DeviceTrustService;

// Singleton that owns a single DeviceTrustService instance.
class DeviceTrustFactory : public BrowserContextKeyedServiceFactory {
 public:
  static DeviceTrustFactory* GetInstance();
  static DeviceTrustService* GetForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<DeviceTrustFactory>;

  DeviceTrustFactory();
  ~DeviceTrustFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_FACTORY_H_

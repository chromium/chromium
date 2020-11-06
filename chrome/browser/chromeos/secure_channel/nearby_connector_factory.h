// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SECURE_CHANNEL_NEARBY_CONNECTOR_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_SECURE_CHANNEL_NEARBY_CONNECTOR_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace chromeos {
namespace secure_channel {

class NearbyConnector;

class NearbyConnectorFactory : public BrowserContextKeyedServiceFactory {
 public:
  static NearbyConnector* GetForProfile(Profile* profile);

  static NearbyConnectorFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<NearbyConnectorFactory>;

  NearbyConnectorFactory();
  NearbyConnectorFactory(const NearbyConnectorFactory&) = delete;
  NearbyConnectorFactory& operator=(const NearbyConnectorFactory&) = delete;
  ~NearbyConnectorFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace secure_channel
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SECURE_CHANNEL_NEARBY_CONNECTOR_FACTORY_H_

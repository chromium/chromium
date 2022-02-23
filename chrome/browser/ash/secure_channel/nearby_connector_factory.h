// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SECURE_CHANNEL_NEARBY_CONNECTOR_FACTORY_H_
#define CHROME_BROWSER_ASH_SECURE_CHANNEL_NEARBY_CONNECTOR_FACTORY_H_

// TODO(https://crbug.com/1164001): move to forward declaration.
#include "ash/services/secure_channel/public/cpp/client/nearby_connector.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace ash {
namespace secure_channel {

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
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace chromeos {
namespace secure_channel {
using ::ash::secure_channel::NearbyConnectorFactory;
}
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_SECURE_CHANNEL_NEARBY_CONNECTOR_FACTORY_H_

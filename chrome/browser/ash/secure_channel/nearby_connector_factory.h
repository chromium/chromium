// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SECURE_CHANNEL_NEARBY_CONNECTOR_FACTORY_H_
#define CHROME_BROWSER_ASH_SECURE_CHANNEL_NEARBY_CONNECTOR_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash {
namespace secure_channel {

class NearbyConnector;

class NearbyConnectorFactory : public ProfileKeyedServiceFactory {
 public:
  static NearbyConnector* GetForProfile(Profile* profile);

  static NearbyConnectorFactory* GetInstance();

 private:
  friend base::NoDestructor<NearbyConnectorFactory>;

  NearbyConnectorFactory();
  NearbyConnectorFactory(const NearbyConnectorFactory&) = delete;
  NearbyConnectorFactory& operator=(const NearbyConnectorFactory&) = delete;
  ~NearbyConnectorFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace secure_channel
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SECURE_CHANNEL_NEARBY_CONNECTOR_FACTORY_H_

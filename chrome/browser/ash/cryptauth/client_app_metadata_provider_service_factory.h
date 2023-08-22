// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CRYPTAUTH_CLIENT_APP_METADATA_PROVIDER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_CRYPTAUTH_CLIENT_APP_METADATA_PROVIDER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash {

class ClientAppMetadataProviderService;

// Factory which creates one ClientAppMetadataProviderService per profile.
class ClientAppMetadataProviderServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static ClientAppMetadataProviderService* GetForProfile(Profile* profile);
  static ClientAppMetadataProviderServiceFactory* GetInstance();

  ClientAppMetadataProviderServiceFactory(
      const ClientAppMetadataProviderServiceFactory&) = delete;
  ClientAppMetadataProviderServiceFactory& operator=(
      const ClientAppMetadataProviderServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ClientAppMetadataProviderServiceFactory>;

  ClientAppMetadataProviderServiceFactory();
  ~ClientAppMetadataProviderServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* browser_context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CRYPTAUTH_CLIENT_APP_METADATA_PROVIDER_SERVICE_FACTORY_H_

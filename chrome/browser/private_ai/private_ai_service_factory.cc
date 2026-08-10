// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_ai/private_ai_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "components/private_ai/content/private_ai_network_driver_content.h"
#include "components/private_ai/content/private_ai_oak_session_driver_content.h"
#include "components/private_ai/features.h"
#include "components/private_ai/phosphor/blind_sign_auth_factory_impl.h"
#include "components/private_ai/private_ai_service.h"
#include "content/public/browser/storage_partition.h"

namespace private_ai {

// static
PrivateAiService* PrivateAiServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<PrivateAiService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
PrivateAiServiceFactory* PrivateAiServiceFactory::GetInstance() {
  static base::NoDestructor<PrivateAiServiceFactory> instance;
  return instance.get();
}

PrivateAiServiceFactory::PrivateAiServiceFactory()
    : ProfileKeyedServiceFactory("private_ai::PrivateAiService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

PrivateAiServiceFactory::~PrivateAiServiceFactory() = default;

std::unique_ptr<KeyedService>
PrivateAiServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  version_info::Channel channel = chrome::GetChannel();
  if (!PrivateAiService::CanPrivateAiBeEnabled(channel)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);

  auto network_driver = std::make_unique<PrivateAiNetworkDriverContent>();
  auto oak_session_driver =
      std::make_unique<PrivateAiOakSessionDriverContent>();

  auto bsa_factory = std::make_unique<phosphor::BlindSignAuthFactoryImpl>();
  return std::make_unique<PrivateAiService>(
      IdentityManagerFactory::GetForProfile(profile), bsa_factory.get(),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      std::move(network_driver), std::move(oak_session_driver),
      profile->GetDefaultStoragePartition()->GetNetworkContext(),
      kPrivateAiUrl.Get(), PrivateAiService::GetApiKey(channel),
      kPrivateAiProxyServerUrl.Get(),
      base::FeatureList::IsEnabled(kPrivateAiUseTokenAttestation), channel);
}

}  // namespace private_ai

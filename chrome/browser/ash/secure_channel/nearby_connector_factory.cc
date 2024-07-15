// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/secure_channel/nearby_connector_factory.h"

#include "chrome/browser/ash/nearby/nearby_process_manager_factory.h"
#include "chrome/browser/ash/secure_channel/nearby_connector_impl.h"
#include "chrome/browser/ash/secure_channel/secure_channel_client_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/nearby_connector.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"

namespace ash {
namespace secure_channel {

// static
NearbyConnector* NearbyConnectorFactory::GetForProfile(Profile* profile) {
  return static_cast<NearbyConnectorImpl*>(
      NearbyConnectorFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
NearbyConnectorFactory* NearbyConnectorFactory::GetInstance() {
  static base::NoDestructor<NearbyConnectorFactory> instance;
  return instance.get();
}

NearbyConnectorFactory::NearbyConnectorFactory()
    : ProfileKeyedServiceFactory(
          "NearbyConnector",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(nearby::NearbyProcessManagerFactory::GetInstance());
}

NearbyConnectorFactory::~NearbyConnectorFactory() = default;

std::unique_ptr<KeyedService>
NearbyConnectorFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  nearby::NearbyProcessManager* nearby_process_manager =
      nearby::NearbyProcessManagerFactory::GetForProfile(
          Profile::FromBrowserContext(context));

  // If null, control of the Nearby utility process is not supported for this
  // profile.
  if (!nearby_process_manager)
    return nullptr;

  auto nearby_connector =
      std::make_unique<NearbyConnectorImpl>(nearby_process_manager);
  SecureChannelClientProvider::GetInstance()->GetClient()->SetNearbyConnector(
      nearby_connector.get());
  return nearby_connector;
}

bool NearbyConnectorFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace secure_channel
}  // namespace ash

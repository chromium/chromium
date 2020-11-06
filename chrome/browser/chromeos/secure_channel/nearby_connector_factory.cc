// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/secure_channel/nearby_connector_factory.h"

#include "chrome/browser/chromeos/nearby/nearby_process_manager_factory.h"
#include "chrome/browser/chromeos/secure_channel/nearby_connector_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {
namespace secure_channel {

// static
NearbyConnector* NearbyConnectorFactory::GetForProfile(Profile* profile) {
  return static_cast<NearbyConnectorImpl*>(
      NearbyConnectorFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
NearbyConnectorFactory* NearbyConnectorFactory::GetInstance() {
  return base::Singleton<NearbyConnectorFactory>::get();
}

NearbyConnectorFactory::NearbyConnectorFactory()
    : BrowserContextKeyedServiceFactory(
          "NearbyConnector",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(nearby::NearbyProcessManagerFactory::GetInstance());
}

NearbyConnectorFactory::~NearbyConnectorFactory() = default;

KeyedService* NearbyConnectorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  nearby::NearbyProcessManager* nearby_process_manager =
      nearby::NearbyProcessManagerFactory::GetForProfile(
          Profile::FromBrowserContext(context));

  // If null, control of the Nearby utility process is not supported for this
  // profile.
  if (!nearby_process_manager)
    return nullptr;

  return new NearbyConnectorImpl(nearby_process_manager);
}

bool NearbyConnectorFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace secure_channel
}  // namespace chromeos

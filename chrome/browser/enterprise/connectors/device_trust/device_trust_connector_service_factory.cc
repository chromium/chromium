// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"

namespace enterprise_connectors {

// static
DeviceTrustConnectorServiceFactory*
DeviceTrustConnectorServiceFactory::GetInstance() {
  return base::Singleton<DeviceTrustConnectorServiceFactory>::get();
}

// static
DeviceTrustConnectorService* DeviceTrustConnectorServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<DeviceTrustConnectorService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

bool DeviceTrustConnectorServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  // TODO(b/204914180): Change this when ready to initialize the DT Key Manager.
  return false;
}

DeviceTrustConnectorServiceFactory::DeviceTrustConnectorServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "DeviceTrustConnectorService",
          BrowserContextDependencyManager::GetInstance()) {}

DeviceTrustConnectorServiceFactory::~DeviceTrustConnectorServiceFactory() =
    default;

KeyedService* DeviceTrustConnectorServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  return new DeviceTrustConnectorService(profile->GetPrefs());
}

}  // namespace enterprise_connectors

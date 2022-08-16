// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service_factory.h"

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/policy/core/common/management/management_service.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/desktop_attestation_service.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/enterprise/connectors/device_trust/attestation/ash/ash_attestation_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace enterprise_connectors {

// static
DeviceTrustServiceFactory* DeviceTrustServiceFactory::GetInstance() {
  return base::Singleton<DeviceTrustServiceFactory>::get();
}

// static
DeviceTrustService* DeviceTrustServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<DeviceTrustService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

DeviceTrustServiceFactory::DeviceTrustServiceFactory()
    : ProfileKeyedServiceFactory("DeviceTrustService") {
  DependsOn(DeviceTrustConnectorServiceFactory::GetInstance());
  DependsOn(PolicyBlocklistFactory::GetInstance());
  DependsOn(policy::ManagementServiceFactory::GetInstance());
}

DeviceTrustServiceFactory::~DeviceTrustServiceFactory() = default;

KeyedService* DeviceTrustServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);

  auto* management_service =
      policy::ManagementServiceFactory::GetForProfile(profile);
  if (!management_service || !management_service->IsManaged()) {
    // Return nullptr since the current management configuration isn't
    // supported.
    return nullptr;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<AttestationService> attestation_service =
      std::make_unique<AshAttestationService>(profile);
#else
  DeviceTrustKeyManager* key_manager = nullptr;
  auto* browser_policy_connector =
      g_browser_process->browser_policy_connector();
  if (browser_policy_connector) {
    auto* cbcm_controller =
        browser_policy_connector->chrome_browser_cloud_management_controller();
    if (cbcm_controller) {
      key_manager = cbcm_controller->GetDeviceTrustKeyManager();
    }
  }

  if (!key_manager) {
    return nullptr;
  }

  std::unique_ptr<AttestationService> attestation_service =
      std::make_unique<DesktopAttestationService>(key_manager);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  auto signals_service = CreateSignalsService(
      profile, PolicyBlocklistFactory::GetForBrowserContext(context),
      management_service);

  if (!signals_service) {
    return nullptr;
  }

  auto* dt_connector_service =
      DeviceTrustConnectorServiceFactory::GetForProfile(profile);

  if (!dt_connector_service) {
    return nullptr;
  }

  // Only return an actual instance if all of the service's dependencies can be
  // resolved (meaning that the current management configuration is supported).
  return new DeviceTrustService(std::move(attestation_service),
                                std::move(signals_service),
                                dt_connector_service);
}

}  // namespace enterprise_connectors

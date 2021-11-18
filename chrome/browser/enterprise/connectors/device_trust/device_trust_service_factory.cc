// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service_factory.h"

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "content/public/browser/browser_context.h"

#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/desktop_attestation_service.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)

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
    : BrowserContextKeyedServiceFactory(
          "DeviceTrustService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(DeviceTrustConnectorServiceFactory::GetInstance());
  DependsOn(PolicyBlocklistFactory::GetInstance());
}

DeviceTrustServiceFactory::~DeviceTrustServiceFactory() = default;

KeyedService* DeviceTrustServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<AttestationService> attestation_service =
      std::make_unique<AshAttestationService>(profile);
#else
  auto* key_manager = g_browser_process->browser_policy_connector()
                          ->chrome_browser_cloud_management_controller()
                          ->GetDeviceTrustKeyManager();
  std::unique_ptr<AttestationService> attestation_service =
      std::make_unique<DesktopAttestationService>(key_manager);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return new DeviceTrustService(
      std::move(attestation_service),
      CreateSignalsService(
          profile, PolicyBlocklistFactory::GetForBrowserContext(context)),
      DeviceTrustConnectorServiceFactory::GetForProfile(profile));
}

}  // namespace enterprise_connectors

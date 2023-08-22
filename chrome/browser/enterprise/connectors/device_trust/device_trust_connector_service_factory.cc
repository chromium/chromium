// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service_factory.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/device_trust/browser/signing_key_policy_observer.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace enterprise_connectors {

// static
DeviceTrustConnectorServiceFactory*
DeviceTrustConnectorServiceFactory::GetInstance() {
  static base::NoDestructor<DeviceTrustConnectorServiceFactory> instance;
  return instance.get();
}

// static
DeviceTrustConnectorService* DeviceTrustConnectorServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<DeviceTrustConnectorService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

bool DeviceTrustConnectorServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return IsDeviceTrustConnectorFeatureEnabled();
#else
  return false;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
}

bool DeviceTrustConnectorServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

DeviceTrustConnectorServiceFactory::DeviceTrustConnectorServiceFactory()
    : ProfileKeyedServiceFactory(
          "DeviceTrustConnectorService",
          ProfileSelections::BuildForRegularAndIncognito()) {}

DeviceTrustConnectorServiceFactory::~DeviceTrustConnectorServiceFactory() =
    default;

std::unique_ptr<KeyedService>
DeviceTrustConnectorServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  // Disallow service for Incognito except for the sign-in profile of ChromeOS
  // (on the login screen).
  if (context->IsOffTheRecord()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (!ash::features::IsLoginScreenDeviceTrustConnectorFeatureEnabled() ||
        !ash::ProfileHelper::IsSigninProfile(profile))
      return nullptr;
#else
    return nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  std::unique_ptr<DeviceTrustConnectorService> service =
      std::make_unique<DeviceTrustConnectorService>(profile->GetPrefs());

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (IsDeviceTrustConnectorFeatureEnabled()) {
    auto* key_manager = g_browser_process->browser_policy_connector()
                            ->chrome_browser_cloud_management_controller()
                            ->GetDeviceTrustKeyManager();
    service->AddObserver(
        std::make_unique<SigningKeyPolicyObserver>(key_manager));
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

  return service;
}

}  // namespace enterprise_connectors

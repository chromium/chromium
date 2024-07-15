// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service_factory.h"

#include <vector>

#include "base/no_destructor.h"
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
#include "components/policy/core/common/management/management_service.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/browser/browser_attestation_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/browser/device_attester.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/browser/profile_attester.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/enterprise/signals/signals_aggregator_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/profile_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/enterprise/connectors/device_trust/ash/ash_attestation_policy_observer.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/ash/ash_attestation_service_impl.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {
bool IsProfileManaged(Profile* profile) {
  auto* management_service =
      policy::ManagementServiceFactory::GetForProfile(profile);
  return management_service && management_service->IsManaged();
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
policy::CloudPolicyStore* GetUserCloudPolicyStore(Profile* profile) {
  policy::CloudPolicyManager* user_policy_manager =
      profile->GetCloudPolicyManager();
  if (user_policy_manager) {
    auto* core = user_policy_manager->core();
    if (core) {
      return core->store();
    }
  }
  return nullptr;
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

}  // namespace

namespace enterprise_connectors {

// static
DeviceTrustServiceFactory* DeviceTrustServiceFactory::GetInstance() {
  static base::NoDestructor<DeviceTrustServiceFactory> instance;
  return instance.get();
}

// static
DeviceTrustService* DeviceTrustServiceFactory::GetForProfile(Profile* profile) {
  // This blocks the factory from associating nullptr with the current context
  // before enrollment.
  // Checking for a testing profile is needed to block unit tests without a
  // proper setup from checking the management service as this can lead to
  // crashes.
  if (profile->AsTestingProfile() || !IsProfileManaged(profile)) {
    // Return nullptr since the current management configuration isn't
    // supported.
    return nullptr;
  }
  return static_cast<DeviceTrustService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

DeviceTrustServiceFactory::DeviceTrustServiceFactory()
    : ProfileKeyedServiceFactory(
          "DeviceTrustService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(DeviceTrustConnectorServiceFactory::GetInstance());
  DependsOn(policy::ManagementServiceFactory::GetInstance());

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // Depends on this service via the SignalsService having a dependency on it.
  DependsOn(enterprise_signals::SignalsAggregatorFactory::GetInstance());
  // Depends on this service via the ProfileAttester having a dependency on it
  // which is used to construct the BrowserAttestationService.
  DependsOn(enterprise::ProfileIdServiceFactory::GetInstance());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

bool DeviceTrustServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

DeviceTrustServiceFactory::~DeviceTrustServiceFactory() = default;

std::unique_ptr<KeyedService>
DeviceTrustServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);

  if (!IsProfileManaged(profile)) {
    // Return nullptr since the current management configuration isn't
    // supported.
    return nullptr;
  }

  auto* dt_connector_service =
      DeviceTrustConnectorServiceFactory::GetForProfile(profile);

  // If `profile` is a OTR profile but not the login profile on ChromeOS login
  // screen. Then `dt_connector_service` will be null. Hence a
  // DeviceTrustService won't be created for OTR profiles besides the one
  // mentioned before.
  if (!dt_connector_service) {
    return nullptr;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<AshAttestationServiceImpl> ash_attestation_service =
      std::make_unique<AshAttestationServiceImpl>(profile);
  dt_connector_service->AddObserver(
      std::make_unique<AshAttestationPolicyObserver>(
          ash_attestation_service->GetWeakPtr()));
  std::unique_ptr<AttestationService> attestation_service =
      std::move(ash_attestation_service);
#else
  DeviceTrustKeyManager* key_manager = nullptr;
  policy::CloudPolicyStore* browser_cloud_policy_store = nullptr;
  auto* browser_policy_connector =
      g_browser_process->browser_policy_connector();
  if (browser_policy_connector) {
    auto* cbcm_controller =
        browser_policy_connector->chrome_browser_cloud_management_controller();
    auto* machine_policy_manager =
        browser_policy_connector->machine_level_user_cloud_policy_manager();
    if (cbcm_controller) {
      key_manager = cbcm_controller->GetDeviceTrustKeyManager();
    }
    if (machine_policy_manager) {
      browser_cloud_policy_store = machine_policy_manager->store();
    }
  }

  if (!key_manager) {
    return nullptr;
  }

  std::vector<std::unique_ptr<Attester>> attesters;
  attesters.push_back(std::make_unique<DeviceAttester>(
      key_manager, policy::BrowserDMTokenStorage::Get(),
      browser_cloud_policy_store));
  attesters.push_back(std::make_unique<ProfileAttester>(
      enterprise::ProfileIdServiceFactory::GetForProfile(profile),
      GetUserCloudPolicyStore(profile)));

  auto attestation_service =
      std::make_unique<BrowserAttestationService>(std::move(attesters));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  auto signals_service = CreateSignalsService(profile);

  if (!signals_service) {
    return nullptr;
  }

  // Only return an actual instance if all of the service's dependencies can be
  // resolved (meaning that the current management configuration is supported).
  return std::make_unique<DeviceTrustService>(std::move(attestation_service),
                                              std::move(signals_service),
                                              dt_connector_service);
}

}  // namespace enterprise_connectors

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/device_cloud_policy_initializer.h"

#include <memory>
#include <utility>

#include "ash/components/cryptohome/cryptohome_parameters.h"
#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_client_factory_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_device_state.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/enrollment_status.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/prefs/pref_service.h"

namespace policy {

namespace {

std::string GetString(const base::Value& dict, base::StringPiece key) {
  DCHECK(dict.is_dict());
  const std::string* value = dict.FindStringKey(key);
  return value ? *value : std::string();
}

}  // namespace

DeviceCloudPolicyInitializer::DeviceCloudPolicyInitializer(
    PrefService* local_state,
    DeviceManagementService* enterprise_service,
    chromeos::InstallAttributes* install_attributes,
    ServerBackedStateKeysBroker* state_keys_broker,
    DeviceCloudPolicyStoreAsh* policy_store,
    DeviceCloudPolicyManagerAsh* policy_manager,
    chromeos::system::StatisticsProvider* statistics_provider)
    : local_state_(local_state),
      enterprise_service_(enterprise_service),
      install_attributes_(install_attributes),
      state_keys_broker_(state_keys_broker),
      policy_store_(policy_store),
      policy_manager_(policy_manager),
      statistics_provider_(statistics_provider) {}

void DeviceCloudPolicyInitializer::SetSystemURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory) {
  system_url_loader_factory_for_testing_ = system_url_loader_factory;
}

DeviceCloudPolicyInitializer::~DeviceCloudPolicyInitializer() {
  DCHECK(!is_initialized_);
}

void DeviceCloudPolicyInitializer::Init() {
  DCHECK(!is_initialized_);

  is_initialized_ = true;
  policy_store_->AddObserver(this);
  state_keys_update_subscription_ = state_keys_broker_->RegisterUpdateCallback(
      base::BindRepeating(&DeviceCloudPolicyInitializer::TryToStartConnection,
                          base::Unretained(this)));
  policy_manager_observer_.Observe(policy_manager_);

  TryToStartConnection();
}

void DeviceCloudPolicyInitializer::Shutdown() {
  DCHECK(is_initialized_);

  policy_store_->RemoveObserver(this);
  state_keys_update_subscription_ = {};
  policy_manager_observer_.Reset();
  is_initialized_ = false;
}

EnrollmentConfig DeviceCloudPolicyInitializer::GetPrescribedEnrollmentConfig()
    const {
  EnrollmentConfig config;

  // Authentication through the attestation mechanism is controlled by a
  // command line switch that either enables it or forces it (meaning that
  // interactive authentication is disabled).
  switch (DeviceCloudPolicyManagerAsh::GetZeroTouchEnrollmentMode()) {
    case ZeroTouchEnrollmentMode::DISABLED:
      // Only use interactive authentication.
      config.auth_mechanism = EnrollmentConfig::AUTH_MECHANISM_INTERACTIVE;
      break;

    case ZeroTouchEnrollmentMode::ENABLED:
      // Use the best mechanism, which may include attestation if available.
      config.auth_mechanism = EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE;
      break;

    case ZeroTouchEnrollmentMode::FORCED:
      // Only use attestation to authenticate since zero-touch is forced.
      config.auth_mechanism = EnrollmentConfig::AUTH_MECHANISM_ATTESTATION;
      break;

    case ZeroTouchEnrollmentMode::HANDS_OFF:
      // Hands-off implies the same authentication method as Forced.
      config.auth_mechanism = EnrollmentConfig::AUTH_MECHANISM_ATTESTATION;
      break;
  }

  // If OOBE is done and we are not enrolled, make sure we only try interactive
  // enrollment.
  const bool oobe_complete =
      local_state_->GetBoolean(ash::prefs::kOobeComplete);
  if (oobe_complete &&
      config.auth_mechanism == EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE)
    config.auth_mechanism = EnrollmentConfig::AUTH_MECHANISM_INTERACTIVE;
  // If OOBE is done and we are enrolled, check for need to recover enrollment.
  // Enrollment recovery is not implemented for Active Directory.
  if (oobe_complete && install_attributes_->IsCloudManaged()) {
    // Regardless what mode is applicable, the enrollment domain is fixed.
    config.management_domain = install_attributes_->GetDomain();

    // Enrollment has completed previously and installation-time attributes
    // are in place. Enrollment recovery is required when the server
    // registration gets lost.
    if (local_state_->GetBoolean(prefs::kEnrollmentRecoveryRequired)) {
      LOG(WARNING) << "Enrollment recovery required according to pref.";
      if (statistics_provider_->GetEnterpriseMachineID().empty())
        LOG(WARNING) << "Postponing recovery because machine id is missing.";
      else
        config.mode = EnrollmentConfig::MODE_RECOVERY;
    }

    return config;
  }

  // OOBE is still running, or it is complete but the device hasn't been
  // enrolled yet. In either case, enrollment should take place if there's a
  // signal present that indicates the device should enroll.

  // Gather enrollment signals from various sources.
  const base::Value* device_state =
      local_state_->GetDictionary(prefs::kServerBackedDeviceState);
  std::string device_state_mode;
  std::string device_state_management_domain;
  absl::optional<bool> is_license_packaged_with_device;
  std::string license_type;

  if (device_state) {
    device_state_mode = GetString(*device_state, kDeviceStateMode);
    device_state_management_domain =
        GetString(*device_state, kDeviceStateManagementDomain);
    is_license_packaged_with_device =
        device_state->FindBoolPath(kDeviceStatePackagedLicense);
    license_type = GetString(*device_state, kDeviceStateLicenseType);
  }

  if (is_license_packaged_with_device) {
    config.is_license_packaged_with_device =
        is_license_packaged_with_device.value();
  } else {
    config.is_license_packaged_with_device = false;
  }

  if (license_type == kDeviceStateLicenseTypeEnterprise) {
    config.license_type = EnrollmentConfig::LicenseType::kEnterprise;
  } else if (license_type == kDeviceStateLicenseTypeEducation) {
    config.license_type = EnrollmentConfig::LicenseType::kEducation;
  } else if (license_type == kDeviceStateLicenseTypeTerminal) {
    config.license_type = EnrollmentConfig::LicenseType::kTerminal;
  } else {
    config.license_type = EnrollmentConfig::LicenseType::kNone;
  }

  const bool pref_enrollment_auto_start_present =
      local_state_->HasPrefPath(prefs::kDeviceEnrollmentAutoStart);
  const bool pref_enrollment_auto_start =
      local_state_->GetBoolean(prefs::kDeviceEnrollmentAutoStart);

  const bool pref_enrollment_can_exit_present =
      local_state_->HasPrefPath(prefs::kDeviceEnrollmentCanExit);
  const bool pref_enrollment_can_exit =
      local_state_->GetBoolean(prefs::kDeviceEnrollmentCanExit);

  const bool oem_is_managed =
      GetMachineFlag(chromeos::system::kOemIsEnterpriseManagedKey, false);
  const bool oem_can_exit_enrollment = GetMachineFlag(
      chromeos::system::kOemCanExitEnterpriseEnrollmentKey, true);

  // Decide enrollment mode. Give precedence to forced variants.
  if (device_state_mode == kDeviceStateRestoreModeReEnrollmentEnforced) {
    config.mode = EnrollmentConfig::MODE_SERVER_FORCED;
    config.management_domain = device_state_management_domain;
  } else if (device_state_mode == kDeviceStateInitialModeEnrollmentEnforced) {
    config.mode = EnrollmentConfig::MODE_INITIAL_SERVER_FORCED;
    config.management_domain = device_state_management_domain;
  } else if (device_state_mode ==
             kDeviceStateRestoreModeReEnrollmentZeroTouch) {
    config.mode = EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED;
    config.auth_mechanism = EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE;
    config.management_domain = device_state_management_domain;
  } else if (device_state_mode == kDeviceStateInitialModeEnrollmentZeroTouch) {
    config.mode = EnrollmentConfig::MODE_ATTESTATION_INITIAL_SERVER_FORCED;
    config.auth_mechanism = EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE;
    config.management_domain = device_state_management_domain;
  } else if (pref_enrollment_auto_start_present && pref_enrollment_auto_start &&
             pref_enrollment_can_exit_present && !pref_enrollment_can_exit) {
    config.mode = EnrollmentConfig::MODE_LOCAL_FORCED;
  } else if (oem_is_managed && !oem_can_exit_enrollment) {
    config.mode = EnrollmentConfig::MODE_LOCAL_FORCED;
  } else if (oobe_complete) {
    // If OOBE is complete, don't return advertised modes as there's currently
    // no way to make sure advertised enrollment only gets shown once.
    config.mode = EnrollmentConfig::MODE_NONE;
  } else if (device_state_mode ==
             kDeviceStateRestoreModeReEnrollmentRequested) {
    config.mode = EnrollmentConfig::MODE_SERVER_ADVERTISED;
    config.management_domain = device_state_management_domain;
  } else if (pref_enrollment_auto_start_present && pref_enrollment_auto_start) {
    config.mode = EnrollmentConfig::MODE_LOCAL_ADVERTISED;
  } else if (oem_is_managed) {
    config.mode = EnrollmentConfig::MODE_LOCAL_ADVERTISED;
  }

  return config;
}

void DeviceCloudPolicyInitializer::OnStoreLoaded(CloudPolicyStore* store) {
  TryToStartConnection();
}

void DeviceCloudPolicyInitializer::OnStoreError(CloudPolicyStore* store) {
  // Do nothing.
}

void DeviceCloudPolicyInitializer::OnDeviceCloudPolicyManagerConnected() {
  // Do nothing.
}
void DeviceCloudPolicyInitializer::OnDeviceCloudPolicyManagerDisconnected() {
  // Do nothing.
}
void DeviceCloudPolicyInitializer::OnDeviceCloudPolicyManagerGotRegistry() {
  // `policy_manager_->HasSchemaRegistry()` is one of requirements for
  // StartConnection. Make another attempt when `policy_manager_` gets its
  // registry.
  policy_manager_observer_.Reset();
  TryToStartConnection();
}

std::unique_ptr<CloudPolicyClient> DeviceCloudPolicyInitializer::CreateClient(
    DeviceManagementService* device_management_service) {
  // DeviceDMToken callback is empty here because for device policies this
  // DMToken is already provided in the policy fetch requests.
  return CreateDeviceCloudPolicyClientAsh(
      statistics_provider_, device_management_service,
      system_url_loader_factory_for_testing_
          ? system_url_loader_factory_for_testing_
          : g_browser_process->shared_url_loader_factory(),
      CloudPolicyClient::DeviceDMTokenCallback());
}

void DeviceCloudPolicyInitializer::TryToStartConnection() {
  if (install_attributes_->IsActiveDirectoryManaged()) {
    // This will go away once ChromeAd deprecation is completed.
    return;
  }

  if (!policy_store_->is_initialized() || !policy_store_->has_policy()) {
    return;
  }

  if (!policy_manager_store_ready_notified_) {
    policy_manager_store_ready_notified_ = true;
    policy_manager_->OnPolicyStoreReady(install_attributes_);
  }

  // TODO(crbug.com/1304636): Move this and all other checks from here to a
  // separate method.
  if (!policy_manager_->HasSchemaRegistry()) {
    // crbug.com/1295871: `policy_manager_` might not have schema registry on
    // start connection attempt. This may happen on chrome restart when
    // `chrome::kInitialProfile` is created after login profile: policy will be
    // loaded but `BuildSchemaRegistryServiceForProfile` will not be called for
    // non-initial / non-sign-in profile.
    return;
  }

  // Currently reven devices don't support sever-backed state keys, but they
  // also don't support FRE/AutoRE so don't block initialization of device
  // policy on state keys being available on reven.
  // TODO(b/208705225): Remove this special case when reven supports state keys.
  const bool allow_init_without_state_keys = ash::switches::IsRevenBranding();

  // TODO(b/181140445): If we had a separate state keys upload request to DM
  // Server we could drop the `state_keys_broker_->available()` requirement.
  if (allow_init_without_state_keys || state_keys_broker_->available()) {
    StartConnection(CreateClient(enterprise_service_));
  }
}

void DeviceCloudPolicyInitializer::StartConnection(
    std::unique_ptr<CloudPolicyClient> client) {
  // This initializer will be deleted once `policy_manager_` is connected.
  // Stop observing the manager as there's nothing interesting it can say
  // anymore.
  policy_manager_observer_.Reset();

  if (!policy_manager_->IsConnected()) {
    policy_manager_->StartConnection(std::move(client), install_attributes_);
  }
}

bool DeviceCloudPolicyInitializer::GetMachineFlag(const std::string& key,
                                                  bool default_value) const {
  bool value = default_value;
  if (!statistics_provider_->GetMachineFlag(key, &value))
    return default_value;

  return value;
}

}  // namespace policy

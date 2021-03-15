// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/ash/attestation/attestation_ca_client.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/chromeos/policy/enrollment_handler_chromeos.h"
#include "chrome/browser/chromeos/policy/enrollment_requisition_manager.h"
#include "chrome/browser/chromeos/policy/server_backed_device_state.h"
#include "chrome/browser/chromeos/policy/status_collector/device_status_collector.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/enrollment_status.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "chromeos/attestation/attestation_flow.h"
#include "chromeos/attestation/attestation_flow_utils.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/attestation/attestation.pb.h"
#include "chromeos/dbus/attestation/attestation_client.h"
#include "chromeos/dbus/attestation/interface.pb.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
class ActiveDirectoryJoinDelegate;
}

namespace policy {

namespace {

// Format MAC address from AA:AA:AA:AA:AA:AA into AAAAAAAAAAAA (12 digit string)
void FormatMacAddress(std::string* mac_address) {
  base::ReplaceChars(*mac_address, ":", "", mac_address);
  DCHECK(mac_address->empty() || mac_address->size() == 12);
}

}  // namespace

DeviceCloudPolicyInitializer::DeviceCloudPolicyInitializer(
    PrefService* local_state,
    DeviceManagementService* enterprise_service,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
    chromeos::InstallAttributes* install_attributes,
    ServerBackedStateKeysBroker* state_keys_broker,
    DeviceCloudPolicyStoreChromeOS* policy_store,
    DeviceCloudPolicyManagerChromeOS* policy_manager,
    std::unique_ptr<chromeos::attestation::AttestationFlow> attestation_flow,
    chromeos::system::StatisticsProvider* statistics_provider)
    : local_state_(local_state),
      enterprise_service_(enterprise_service),
      background_task_runner_(background_task_runner),
      install_attributes_(install_attributes),
      state_keys_broker_(state_keys_broker),
      policy_store_(policy_store),
      policy_manager_(policy_manager),
      attestation_flow_(std::move(attestation_flow)),
      statistics_provider_(statistics_provider),
      signing_service_(std::make_unique<TpmEnrollmentKeySigningService>()) {}

void DeviceCloudPolicyInitializer::SetSigningServiceForTesting(
    std::unique_ptr<policy::SigningService> signing_service) {
  signing_service_ = std::move(signing_service);
}

void DeviceCloudPolicyInitializer::SetSystemURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory) {
  system_url_loader_factory_for_testing_ = system_url_loader_factory;
}

void DeviceCloudPolicyInitializer::SetAttestationFlowForTesting(
    std::unique_ptr<chromeos::attestation::AttestationFlow> attestation_flow) {
  attestation_flow_ = std::move(attestation_flow);
}

DeviceCloudPolicyInitializer::~DeviceCloudPolicyInitializer() {
  DCHECK(!is_initialized_);
}

void DeviceCloudPolicyInitializer::Init() {
  DCHECK(!is_initialized_);

  is_initialized_ = true;
  policy_store_->AddObserver(this);
  state_keys_update_subscription_ =
      state_keys_broker_->RegisterUpdateCallback(base::BindRepeating(
          &DeviceCloudPolicyInitializer::TryToCreateClient,
          base::Unretained(this), StartConnectionReason::kStateKeysStored));

  TryToCreateClient(StartConnectionReason::kInitialCreation);
}

void DeviceCloudPolicyInitializer::Shutdown() {
  DCHECK(is_initialized_);

  policy_store_->RemoveObserver(this);
  enrollment_handler_.reset();
  state_keys_update_subscription_ = {};
  is_initialized_ = false;
}

void DeviceCloudPolicyInitializer::PrepareEnrollment(
    DeviceManagementService* device_management_service,
    chromeos::ActiveDirectoryJoinDelegate* ad_join_delegate,
    const EnrollmentConfig& enrollment_config,
    DMAuth dm_auth,
    EnrollmentCallback enrollment_callback) {
  DCHECK(is_initialized_);
  DCHECK(!enrollment_handler_);

  policy_manager_->core()->Disconnect();

  enrollment_handler_.reset(new EnrollmentHandlerChromeOS(
      policy_store_, install_attributes_, state_keys_broker_,
      attestation_flow_.get(), CreateClient(device_management_service),
      background_task_runner_, ad_join_delegate, enrollment_config,
      std::move(dm_auth), install_attributes_->GetDeviceId(),
      EnrollmentRequisitionManager::GetDeviceRequisition(),
      EnrollmentRequisitionManager::GetSubOrganization(),
      base::BindOnce(&DeviceCloudPolicyInitializer::EnrollmentCompleted,
                     base::Unretained(this), std::move(enrollment_callback))));
}

void DeviceCloudPolicyInitializer::StartEnrollment() {
  DCHECK(is_initialized_);
  DCHECK(enrollment_handler_);
  enrollment_handler_->StartEnrollment();
}

EnrollmentConfig DeviceCloudPolicyInitializer::GetPrescribedEnrollmentConfig()
    const {
  EnrollmentConfig config;

  // Authentication through the attestation mechanism is controlled by a
  // command line switch that either enables it or forces it (meaning that
  // interactive authentication is disabled).
  switch (DeviceCloudPolicyManagerChromeOS::GetZeroTouchEnrollmentMode()) {
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
      local_state_->GetBoolean(chromeos::prefs::kOobeComplete);
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
  const base::DictionaryValue* device_state =
      local_state_->GetDictionary(prefs::kServerBackedDeviceState);
  std::string device_state_mode;
  std::string device_state_management_domain;
  base::Optional<bool> is_license_packaged_with_device;

  if (device_state) {
    device_state->GetString(kDeviceStateMode, &device_state_mode);
    device_state->GetString(kDeviceStateManagementDomain,
                            &device_state_management_domain);
    is_license_packaged_with_device =
        device_state->FindBoolPath(kDeviceStatePackagedLicense);
  }

  if (is_license_packaged_with_device) {
    config.is_license_packaged_with_device =
        is_license_packaged_with_device.value();
  } else {
    config.is_license_packaged_with_device = false;
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
  TryToCreateClient(StartConnectionReason::kCloudPolicyLoaded);
}

void DeviceCloudPolicyInitializer::OnStoreError(CloudPolicyStore* store) {
  // Do nothing.
}

void DeviceCloudPolicyInitializer::EnrollmentCompleted(
    EnrollmentCallback enrollment_callback,
    EnrollmentStatus status) {
  std::unique_ptr<CloudPolicyClient> client =
      enrollment_handler_->ReleaseClient();
  enrollment_handler_.reset();

  if (status.status() == EnrollmentStatus::SUCCESS &&
      !install_attributes_->IsActiveDirectoryManaged()) {
    StartConnection(StartConnectionReason::kEnrollmentCompleted,
                    std::move(client));
  } else {
    // Some attempts to create a client may be blocked because the enrollment
    // was in progress. We give it a try again.
    TryToCreateClient(StartConnectionReason::kEnrollmentCompleted);
  }

  if (!enrollment_callback.is_null())
    std::move(enrollment_callback).Run(status);
}

std::unique_ptr<CloudPolicyClient> DeviceCloudPolicyInitializer::CreateClient(
    DeviceManagementService* device_management_service) {
  std::string machine_model;
  statistics_provider_->GetMachineStatistic(chromeos::system::kHardwareClassKey,
                                            &machine_model);
  std::string brand_code;
  statistics_provider_->GetMachineStatistic(chromeos::system::kRlzBrandCodeKey,
                                            &brand_code);
  std::string attested_device_id;
  statistics_provider_->GetMachineStatistic(
      chromeos::system::kAttestedDeviceIdKey, &attested_device_id);
  // The :'s should be removed from MAC addresses to match the format of
  // reporting MAC addresses and corresponding VPD fields.
  std::string ethernet_mac_address;
  statistics_provider_->GetMachineStatistic(
      chromeos::system::kEthernetMacAddressKey, &ethernet_mac_address);
  FormatMacAddress(&ethernet_mac_address);
  std::string dock_mac_address;
  statistics_provider_->GetMachineStatistic(
      chromeos::system::kDockMacAddressKey, &dock_mac_address);
  FormatMacAddress(&dock_mac_address);
  std::string manufacture_date;
  statistics_provider_->GetMachineStatistic(
      chromeos::system::kManufactureDateKey, &manufacture_date);
  // DeviceDMToken callback is empty here because for device policies this
  // DMToken is already provided in the policy fetch requests.
  return std::make_unique<CloudPolicyClient>(
      statistics_provider_->GetEnterpriseMachineID(), machine_model, brand_code,
      attested_device_id, ethernet_mac_address, dock_mac_address,
      manufacture_date, signing_service_.get(), device_management_service,
      system_url_loader_factory_for_testing_
          ? system_url_loader_factory_for_testing_
          : g_browser_process->shared_url_loader_factory(),
      CloudPolicyClient::DeviceDMTokenCallback());
}

void DeviceCloudPolicyInitializer::TryToCreateClient(
    DeviceCloudPolicyInitializer::StartConnectionReason reason) {
  if (!policy_store_->is_initialized() || !policy_store_->has_policy() ||
      !state_keys_broker_->available() || enrollment_handler_ ||
      install_attributes_->IsActiveDirectoryManaged()) {
    return;
  }
  StartConnection(reason, CreateClient(enterprise_service_));
}

void DeviceCloudPolicyInitializer::StartConnection(
    DeviceCloudPolicyInitializer::StartConnectionReason reason,
    std::unique_ptr<CloudPolicyClient> client) {
  if (!policy_manager_->core()->service()) {
    LOG(WARNING)
        << "DeviceCloudPolicyInitializer will be removed soon with reason: "
        << static_cast<int>(reason);
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

DeviceCloudPolicyInitializer::TpmEnrollmentKeySigningService::
    TpmEnrollmentKeySigningService() = default;

DeviceCloudPolicyInitializer::TpmEnrollmentKeySigningService::
    ~TpmEnrollmentKeySigningService() = default;

void DeviceCloudPolicyInitializer::TpmEnrollmentKeySigningService::SignData(
    const std::string& data,
    SigningCallback callback) {
  const chromeos::attestation::AttestationCertificateProfile cert_profile =
      chromeos::attestation::PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE;
  ::attestation::SignSimpleChallengeRequest request;
  request.set_username("");
  request.set_key_label(
      chromeos::attestation::GetKeyNameForProfile(cert_profile, ""));
  request.set_challenge(data);
  chromeos::AttestationClient::Get()->SignSimpleChallenge(
      request, base::BindOnce(&DeviceCloudPolicyInitializer::
                                  TpmEnrollmentKeySigningService::OnDataSigned,
                              weak_ptr_factory_.GetWeakPtr(), data,
                              std::move(callback)));
}

void DeviceCloudPolicyInitializer::TpmEnrollmentKeySigningService::OnDataSigned(
    const std::string& data,
    SigningCallback callback,
    const ::attestation::SignSimpleChallengeReply& reply) {
  enterprise_management::SignedData em_signed_data;
  chromeos::attestation::SignedData att_signed_data;
  const bool success =
      reply.status() == ::attestation::STATUS_SUCCESS &&
      att_signed_data.ParseFromString(reply.challenge_response());
  if (success) {
    em_signed_data.set_data(att_signed_data.data());
    em_signed_data.set_signature(att_signed_data.signature());
    em_signed_data.set_extra_data_bytes(att_signed_data.data().size() -
                                        data.size());
  }
  std::move(callback).Run(success, em_signed_data);
}

}  // namespace policy

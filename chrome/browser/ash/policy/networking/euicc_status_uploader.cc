// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/networking/euicc_status_uploader.h"

#include "ash/constants/ash_features.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram_functions.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler.h"
#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace policy {

namespace {

const char kLastUploadedEuiccStatusEuiccCountKey[] = "euicc_count";
const char kLastUploadedEuiccStatusESimProfilesKey[] = "esim_profiles";
const char kLastUploadedEuiccStatusESimProfilesIccidKey[] = "iccid";
const char kLastUploadedEuiccStatusESimProfilesNetworkNameKey[] =
    "network_name";
const char kLastUploadedEuiccStatusESimProfilesSmdpActivationCodeKey[] =
    "smdp_activation_code";
const char kLastUploadedEuiccStatusESimProfilesSmdsActivationCodeKey[] =
    "smds_activation_code";

const net::BackoffEntry::Policy kBackOffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms.
    static_cast<int>(base::Minutes(5).InMilliseconds()),

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.5,
    // Maximum amount of time we are willing to delay our request in ms.
    static_cast<int>(base::Hours(6).InMilliseconds()),

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Starts with initial delay.
    true,
};

// Returns whether the device's policy data is active and provisioned.
bool IsDeviceManaged() {
  return ::ash::DeviceSettingsService::IsInitialized() &&
         ::ash::DeviceSettingsService::Get()->policy_data() &&
         ::ash::DeviceSettingsService::Get()->policy_data()->state() ==
             enterprise_management::PolicyData::ACTIVE;
}

}  // namespace

// static
const char EuiccStatusUploader::kLastUploadedEuiccStatusPref[] =
    "esim.last_uploaded_euicc_status";
const char EuiccStatusUploader::kShouldSendClearProfilesRequestPref[] =
    "esim.should_clear_profile_list";

EuiccStatusUploader::EuiccStatusUploader(CloudPolicyClient* client,
                                         PrefService* local_state)
    : EuiccStatusUploader(client,
                          local_state,
                          base::BindRepeating(&IsDeviceManaged)) {}

EuiccStatusUploader::EuiccStatusUploader(
    CloudPolicyClient* client,
    PrefService* local_state,
    IsDeviceActiveCallback is_device_active_callback)
    : client_(client),
      local_state_(local_state),
      is_device_managed_callback_(std::move(is_device_active_callback)),
      retry_entry_(&kBackOffPolicy) {
  if (!ash::NetworkHandler::IsInitialized()) {
    LOG(WARNING) << "NetworkHandler is not initialized.";
    return;
  }

  hermes_manager_observation_.Observe(ash::HermesManagerClient::Get());
  hermes_euicc_observation_.Observe(ash::HermesEuiccClient::Get());
  cloud_policy_client_observation_.Observe(client_.get());

  auto* network_handler = ash::NetworkHandler::Get();
  network_handler->managed_cellular_pref_handler()->AddObserver(this);
  managed_network_configuration_handler_ =
      network_handler->managed_network_configuration_handler();
  managed_network_configuration_handler_->AddObserver(this);
}

EuiccStatusUploader::~EuiccStatusUploader() {
  if (ash::NetworkHandler::IsInitialized()) {
    ash::NetworkHandler::Get()->managed_cellular_pref_handler()->RemoveObserver(
        this);
  }
  OnManagedNetworkConfigurationHandlerShuttingDown();
}

// static
void EuiccStatusUploader::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kLastUploadedEuiccStatusPref,
                                   PrefRegistry::NO_REGISTRATION_FLAGS);
  registry->RegisterBooleanPref(kShouldSendClearProfilesRequestPref,
                                /*default_value=*/false);
}

// static
std::unique_ptr<enterprise_management::UploadEuiccInfoRequest>
EuiccStatusUploader::ConstructRequestFromStatus(const base::Value::Dict& status,
                                                bool clear_profile_list) {
  auto upload_request =
      std::make_unique<enterprise_management::UploadEuiccInfoRequest>();
  upload_request->set_euicc_count(
      status.FindInt(kLastUploadedEuiccStatusEuiccCountKey).value());

  auto* mutable_esim_profiles = upload_request->mutable_esim_profiles();
  for (const auto& esim_profile :
       *status.FindListByDottedPath(kLastUploadedEuiccStatusESimProfilesKey)) {
    const base::Value::Dict& esim_profile_dict = esim_profile.GetDict();
    enterprise_management::ESimProfileInfo esim_profile_info;
    esim_profile_info.set_iccid(*esim_profile_dict.FindString(
        kLastUploadedEuiccStatusESimProfilesIccidKey));

    const std::string* network_name = esim_profile_dict.FindString(
        kLastUploadedEuiccStatusESimProfilesNetworkNameKey);
    if (network_name && !network_name->empty()) {
      esim_profile_info.set_name(*network_name);
    }

    const std::string* smdp_activation_code = esim_profile_dict.FindString(
        kLastUploadedEuiccStatusESimProfilesSmdpActivationCodeKey);
    const std::string* smds_activation_code = esim_profile_dict.FindString(
        kLastUploadedEuiccStatusESimProfilesSmdsActivationCodeKey);

    if (smdp_activation_code && !smdp_activation_code->empty()) {
      esim_profile_info.set_smdp_address(*smdp_activation_code);
    } else if (smds_activation_code && !smds_activation_code->empty()) {
      esim_profile_info.set_smds_address(*smds_activation_code);
    } else {
      NET_LOG(ERROR) << "Failed to find an activation code when constructing "
                        "EUICC upload request";
      continue;
    }

    mutable_esim_profiles->Add(std::move(esim_profile_info));
  }
  upload_request->set_clear_profile_list(clear_profile_list);
  return upload_request;
}

void EuiccStatusUploader::OnManagedNetworkConfigurationHandlerShuttingDown() {
  if (managed_network_configuration_handler_ &&
      managed_network_configuration_handler_->HasObserver(this)) {
    managed_network_configuration_handler_->RemoveObserver(this);
  }
  managed_network_configuration_handler_ = nullptr;
}

void EuiccStatusUploader::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  MaybeUploadStatus();
}

void EuiccStatusUploader::OnPolicyFetched(CloudPolicyClient* client) {
  if (is_policy_fetched_) {
    return;
  }
  is_policy_fetched_ = true;
  MaybeUploadStatus();
}

void EuiccStatusUploader::PoliciesApplied(const std::string& userhash) {
  MaybeUploadStatus();
}

void EuiccStatusUploader::OnManagedCellularPrefChanged() {
  MaybeUploadStatus();
}

void EuiccStatusUploader::OnAvailableEuiccListChanged() {
  MaybeUploadStatus();
}

void EuiccStatusUploader::OnEuiccReset(const dbus::ObjectPath& euicc_path) {
  // Remember that we should clear the profile list in the next upload. This
  // ensures that profile list will be eventually cleared even if the immediate
  // uploads do not succeed.
  local_state_->SetBoolean(kShouldSendClearProfilesRequestPref, true);
  MaybeUploadStatus();
}

base::Value::Dict EuiccStatusUploader::GetCurrentEuiccStatus() const {
  auto status = base::Value::Dict().Set(
      kLastUploadedEuiccStatusEuiccCountKey,
      static_cast<int>(
          ash::HermesManagerClient::Get()->GetAvailableEuiccs().size()));

  base::Value::List esim_profiles;

  for (const auto& esim_profile : ash::NetworkHandler::Get()
                                      ->cellular_esim_profile_handler()
                                      ->GetESimProfiles()) {
    // Do not report non-provisioned cellular networks.
    if (esim_profile.iccid().empty()) {
      continue;
    }

    const base::Value::Dict* esim_metadata =
        ash::NetworkHandler::Get()
            ->managed_cellular_pref_handler()
            ->GetESimMetadata(esim_profile.iccid());

    // Report only managed profiles that we have metadata for.
    if (!esim_metadata) {
      continue;
    }

    base::Value::Dict esim_profile_to_add;
    esim_profile_to_add.Set(kLastUploadedEuiccStatusESimProfilesIccidKey,
                            esim_profile.iccid());

    const std::string* const smdp_activation_code =
        esim_metadata->FindString(::onc::cellular::kSMDPAddress);
    const std::string* const smds_activation_code =
        esim_metadata->FindString(::onc::cellular::kSMDSAddress);

    if (smdp_activation_code && !smdp_activation_code->empty()) {
      esim_profile_to_add.Set(
          kLastUploadedEuiccStatusESimProfilesSmdpActivationCodeKey,
          *smdp_activation_code);
    } else if (smds_activation_code && !smds_activation_code->empty()) {
      esim_profile_to_add.Set(
          kLastUploadedEuiccStatusESimProfilesSmdsActivationCodeKey,
          *smds_activation_code);
    } else {
      // Report only managed profiles that we have an activation code for.
      NET_LOG(ERROR) << "Failed to find an SM-DP+ or SM-DS activation code "
                     << "in the eSIM metadata, skipping entry";
      continue;
    }

    const std::string* network_name =
        esim_metadata->FindString(::onc::network_config::kName);
    if (network_name && !network_name->empty()) {
      esim_profile_to_add.Set(
          kLastUploadedEuiccStatusESimProfilesNetworkNameKey, *network_name);
    }

    esim_profiles.Append(std::move(esim_profile_to_add));
  }

  status.SetByDottedPath(kLastUploadedEuiccStatusESimProfilesKey,
                         std::move(esim_profiles));
  return status;
}

void EuiccStatusUploader::MaybeUploadStatus() {
  if (!client_->is_registered()) {
    VLOG(1) << "Policy client is not registered.";
    return;
  }

  if (!is_policy_fetched_) {
    VLOG(1) << "Policy not fetched yet.";
    return;
  }

  if (!is_device_managed_callback_.Run()) {
    VLOG(1) << "Device is unmanaged or deprovisioned.";
    return;
  }

  if (!managed_network_configuration_handler_) {
    LOG(WARNING) << "ManageNetworkConfigurationHandler is not initialized.";
    return;
  }

  if (ash::HermesManagerClient::Get()->GetAvailableEuiccs().empty()) {
    VLOG(1) << "No EUICC available on the device.";
    return;
  }

  const base::Value::Dict& last_uploaded_pref =
      local_state_->GetDict(kLastUploadedEuiccStatusPref);
  auto current_state = GetCurrentEuiccStatus();

  // Force send the status if reset request was received.
  if (local_state_->GetBoolean(kShouldSendClearProfilesRequestPref)) {
    UploadStatus(std::move(current_state));
    return;
  }

  if (attempted_upload_status_ == current_state) {
    // We attempted to upload this status, but failed.
    // Schedule retry.
    if (!retry_timer_) {
      retry_timer_ = std::make_unique<base::OneShotTimer>();
      retry_timer_->Start(FROM_HERE, retry_entry_.GetTimeUntilRelease(),
                          base::BindOnce(&EuiccStatusUploader::RetryUpload,
                                         weak_ptr_factory_.GetWeakPtr()));
    }
    return;
  }

  retry_timer_.reset();

  if (last_uploaded_pref != current_state) {
    UploadStatus(std::move(current_state));
  }
}

void EuiccStatusUploader::UploadStatus(base::Value::Dict status) {
  // Do not upload anything until the current upload finishes.
  if (currently_uploading_) {
    return;
  }
  currently_uploading_ = true;
  attempted_upload_status_ = std::move(status);

  const bool should_send_clear_profiles_request =
      local_state_->GetBoolean(kShouldSendClearProfilesRequestPref);

  auto upload_request = ConstructRequestFromStatus(
      attempted_upload_status_, should_send_clear_profiles_request);
  client_->UploadEuiccInfo(
      std::move(upload_request),
      base::BindOnce(&EuiccStatusUploader::OnStatusUploaded,
                     weak_ptr_factory_.GetWeakPtr(),
                     should_send_clear_profiles_request));
}

void EuiccStatusUploader::OnStatusUploaded(
    bool should_send_clear_profiles_request,
    bool success) {
  currently_uploading_ = false;
  retry_entry_.InformOfRequest(/*succeeded=*/success);
  base::UmaHistogramBoolean(
      "Network.Cellular.ESim.Policy.EuiccStatusUploadResult", success);

  if (!success) {
    LOG(ERROR) << "Failed to upload EUICC status.";
    MaybeUploadStatus();
    return;
  }

  VLOG(1) << "EUICC status successfully uploaded.";

  // Remember the last uploaded status to not upload it again.
  local_state_->SetDict(kLastUploadedEuiccStatusPref,
                        std::move(attempted_upload_status_));

  if (should_send_clear_profiles_request) {
    // Clean out the local state preference to not send `clear_profile_list` =
    // true multiple times.
    local_state_->ClearPref(kShouldSendClearProfilesRequestPref);
  }
  attempted_upload_status_.clear();

  MaybeUploadStatus();
}

void EuiccStatusUploader::RetryUpload() {
  attempted_upload_status_.clear();
  MaybeUploadStatus();
}

void EuiccStatusUploader::FireRetryTimerIfExistsForTesting() {
  if (retry_timer_) {
    retry_timer_->FireNow();
  }
}

}  // namespace policy

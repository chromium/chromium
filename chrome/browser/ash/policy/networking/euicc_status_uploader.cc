// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/networking/euicc_status_uploader.h"

#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram_functions.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace policy {

namespace {
const char kLastUploadedEuiccStatusEuiccCountKey[] = "euicc_count";
const char kLastUploadedEuiccStatusESimProfilesKey[] = "esim_profiles";
const char kLastUploadedEuiccStatusESimProfilesIccidKey[] = "iccid";
const char kLastUploadedEuiccStatusESimProfilesSmdpAddressKey[] =
    "smdp_address";
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
    "esim.last_upload_euicc_status";
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
  if (!chromeos::NetworkHandler::IsInitialized()) {
    LOG(WARNING) << "NetworkHandler is not initialized.";
    return;
  }

  hermes_manager_observation_.Observe(chromeos::HermesManagerClient::Get());
  hermes_euicc_observation_.Observe(chromeos::HermesEuiccClient::Get());
  cloud_policy_client_observation_.Observe(client_);

  network_handler_ = chromeos::NetworkHandler::Get();
  network_handler_->managed_network_configuration_handler()->AddObserver(this);
  network_handler_->network_state_handler()->AddObserver(this, FROM_HERE);
}

EuiccStatusUploader::~EuiccStatusUploader() {
  if (network_handler_)
    OnShuttingDown();
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
EuiccStatusUploader::ConstructRequestFromStatus(const base::Value& status,
                                                bool clear_profile_list) {
  auto upload_request =
      std::make_unique<enterprise_management::UploadEuiccInfoRequest>();
  upload_request->set_euicc_count(
      status.FindIntKey(kLastUploadedEuiccStatusEuiccCountKey).value());

  auto* mutable_esim_profiles = upload_request->mutable_esim_profiles();
  for (const auto& esim_profile :
       status.FindListPath(kLastUploadedEuiccStatusESimProfilesKey)
           ->GetListDeprecated()) {
    enterprise_management::ESimProfileInfo esim_profile_info;
    esim_profile_info.set_iccid(*esim_profile.FindStringKey(
        kLastUploadedEuiccStatusESimProfilesIccidKey));
    esim_profile_info.set_smdp_address(*esim_profile.FindStringKey(
        kLastUploadedEuiccStatusESimProfilesSmdpAddressKey));
    mutable_esim_profiles->Add(std::move(esim_profile_info));
  }
  upload_request->set_clear_profile_list(clear_profile_list);
  return upload_request;
}

void EuiccStatusUploader::OnShuttingDown() {
  network_handler_->network_state_handler()->RemoveObserver(this, FROM_HERE);
  network_handler_->managed_network_configuration_handler()->RemoveObserver(
      this);
  network_handler_ = nullptr;
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

void EuiccStatusUploader::NetworkListChanged() {
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

base::Value EuiccStatusUploader::GetCurrentEuiccStatus() const {
  base::Value status(base::Value::Type::DICTIONARY);

  status.SetIntKey(
      kLastUploadedEuiccStatusEuiccCountKey,
      chromeos::HermesManagerClient::Get()->GetAvailableEuiccs().size());

  base::Value esim_profiles(base::Value::Type::LIST);

  chromeos::NetworkStateHandler::NetworkStateList networks;
  network_handler_->network_state_handler()->GetNetworkListByType(
      ash::NetworkTypePattern::Cellular(),
      /*configured_only=*/false, /*visible_only=*/false,
      /*limit=*/0, &networks);

  onc::ONCSource onc_source = onc::ONC_SOURCE_NONE;
  for (const chromeos::NetworkState* network : networks) {
    // Report only managed profiles.
    if (!network->IsManagedByPolicy())
      continue;

    // Do not report non-provisioned cellular networks.
    if (network->iccid().empty())
      continue;

    // Read the SMDP address from ONC.
    const base::Value* policy =
        network_handler_->managed_network_configuration_handler()
            ->FindPolicyByGUID(
                /*userhash=*/std::string(), network->guid(), &onc_source);
    if (!policy) {
      NET_LOG(EVENT) << "No device policy found for network guid: "
                     << network->guid();
      continue;
    }

    const base::Value* cellular_dict =
        policy->FindDictKey(::onc::network_config::kCellular);
    if (!cellular_dict) {
      NET_LOG(EVENT)
          << "No Cellular properties found in device policy for network guid: "
          << network->guid();
      continue;
    }

    const std::string* smdp_address =
        cellular_dict->FindStringKey(::onc::cellular::kSMDPAddress);
    if (!smdp_address)
      continue;

    base::Value esim_profile(base::Value::Type::DICTIONARY);
    esim_profile.SetStringKey(kLastUploadedEuiccStatusESimProfilesIccidKey,
                              network->iccid());
    esim_profile.SetStringKey(
        kLastUploadedEuiccStatusESimProfilesSmdpAddressKey, *smdp_address);
    esim_profiles.Append(std::move(esim_profile));
  }

  status.SetPath(kLastUploadedEuiccStatusESimProfilesKey,
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

  if (!network_handler_) {
    LOG(WARNING) << "NetworkHandler is not initialized.";
    return;
  }

  if (chromeos::HermesManagerClient::Get()->GetAvailableEuiccs().empty()) {
    VLOG(1) << "No EUICC available on the device.";
    return;
  }

  const base::Value* last_uploaded_pref =
      local_state_->Get(kLastUploadedEuiccStatusPref);
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

  if (!last_uploaded_pref || *last_uploaded_pref != current_state) {
    UploadStatus(std::move(current_state));
  }
}

void EuiccStatusUploader::UploadStatus(base::Value status) {
  // Do not upload anything until the current upload finishes.
  if (currently_uploading_)
    return;
  currently_uploading_ = true;
  attempted_upload_status_ = std::move(status);

  auto upload_request = ConstructRequestFromStatus(
      attempted_upload_status_,
      local_state_->GetBoolean(kShouldSendClearProfilesRequestPref));
  client_->UploadEuiccInfo(
      std::move(upload_request),
      base::BindOnce(&EuiccStatusUploader::OnStatusUploaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EuiccStatusUploader::OnStatusUploaded(bool success) {
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
  local_state_->Set(kLastUploadedEuiccStatusPref,
                    std::move(attempted_upload_status_));
  // Clean out the local state preference to not send |clear_profile_list| =
  // true multiple times.
  local_state_->ClearPref(kShouldSendClearProfilesRequestPref);
  attempted_upload_status_.DictClear();

  MaybeUploadStatus();
  return;
}

void EuiccStatusUploader::RetryUpload() {
  attempted_upload_status_.DictClear();
  MaybeUploadStatus();
}

void EuiccStatusUploader::FireRetryTimerIfExistsForTesting() {
  if (retry_timer_)
    retry_timer_->FireNow();
}

}  // namespace policy

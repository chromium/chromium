// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/server_backed_state/active_directory_device_state_uploader.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/attestation/enrollment_certificate_uploader_impl.h"
#include "chrome/browser/ash/attestation/enrollment_id_upload_manager.h"
#include "chrome/browser/ash/policy/core/dm_token_storage.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/login_manager/policy_descriptor.pb.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

namespace policy {
namespace {

void MaybeRunStatusCallback(
    ActiveDirectoryDeviceStateUploader::StatusCallback callback,
    bool success) {
  if (callback) {
    std::move(callback).Run(success);
  }
}

}  // namespace

ActiveDirectoryDeviceStateUploader::ActiveDirectoryDeviceStateUploader(
    const std::string& client_id,
    DeviceManagementService* dm_service,
    ServerBackedStateKeysBroker* state_keys_broker,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<DMTokenStorageBase> dm_token_storage,
    PrefService* local_state)
    : client_id_(client_id),
      dm_service_(dm_service),
      state_keys_broker_(state_keys_broker),
      url_loader_factory_(url_loader_factory),
      dm_token_storage_(std::move(dm_token_storage)),
      local_state_(local_state) {}

ActiveDirectoryDeviceStateUploader::~ActiveDirectoryDeviceStateUploader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cloud_policy_client_) {
    Shutdown();
  }
}

// static
void ActiveDirectoryDeviceStateUploader::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kEnrollmentIdUploadedOnChromad,
                                /*default_value=*/false);
}

void ActiveDirectoryDeviceStateUploader::SetStateKeysCallbackForTesting(
    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  state_keys_callback_for_testing_ = std::move(callback);
}

void ActiveDirectoryDeviceStateUploader::SetEnrollmentIdCallbackForTesting(
    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  enrollment_id_callback_for_testing_ = std::move(callback);
}

void ActiveDirectoryDeviceStateUploader::SetDeviceSettingsServiceForTesting(
    ash::DeviceSettingsService* device_settings_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  device_settings_service_for_testing_ = device_settings_service;
}

void ActiveDirectoryDeviceStateUploader::SetEnrollmentIdUploadedForTesting(
    bool value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  local_state_->SetBoolean(prefs::kEnrollmentIdUploadedOnChromad, value);
}

void ActiveDirectoryDeviceStateUploader::
    SetEnrollmentCertificateUploaderForTesting(
        std::unique_ptr<ash::attestation::EnrollmentCertificateUploaderImpl>
            enrollment_certificate_uploader_impl) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  enrollment_certificate_uploader_ =
      std::move(enrollment_certificate_uploader_impl);
}

bool ActiveDirectoryDeviceStateUploader::IsClientRegisteredForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(cloud_policy_client_);
  return cloud_policy_client_->is_registered();
}

CloudPolicyClient*
ActiveDirectoryDeviceStateUploader::CreateClientForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CreateClient();
  return cloud_policy_client_.get();
}

void ActiveDirectoryDeviceStateUploader::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The client may have been created in advance, only in tests.
  if (!cloud_policy_client_) {
    CreateClient();
  }

  cloud_policy_client_->AddObserver(this);
  cloud_policy_client_->AddPolicyTypeToFetch(
      dm_protocol::kChromeDevicePolicyType,
      /*settings_entity_id=*/std::string());

  DCHECK(!state_keys_update_subscription_);
  state_keys_update_subscription_ =
      state_keys_broker_->RegisterUpdateCallback(base::BindRepeating(
          &ActiveDirectoryDeviceStateUploader::OnStateKeysUpdated,
          weak_ptr_factory_.GetWeakPtr()));

  // A custom certificate uploader may have been set in advance, only in tests.
  if (!enrollment_certificate_uploader_) {
    enrollment_certificate_uploader_ =
        std::make_unique<ash::attestation::EnrollmentCertificateUploaderImpl>(
            cloud_policy_client_.get());
  }

  DCHECK(!enrollment_id_upload_manager_);
  enrollment_id_upload_manager_ =
      device_settings_service_for_testing_
          ? std::make_unique<ash::attestation::EnrollmentIdUploadManager>(
                cloud_policy_client_.get(),
                device_settings_service_for_testing_,
                enrollment_certificate_uploader_.get())
          : std::make_unique<ash::attestation::EnrollmentIdUploadManager>(
                cloud_policy_client_.get(),
                enrollment_certificate_uploader_.get());

  // Request DM Token before uploading state keys or enrollment ID.
  CHECK(dm_token_storage_);
  dm_token_storage_->RetrieveDMToken(
      base::BindOnce(&ActiveDirectoryDeviceStateUploader::OnDMTokenAvailable,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ActiveDirectoryDeviceStateUploader::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(cloud_policy_client_);
  cloud_policy_client_->RemoveObserver(this);
  cloud_policy_client_.reset();
  state_keys_update_subscription_ = {};
}

bool ActiveDirectoryDeviceStateUploader::HasUploadedEnrollmentId() const {
  return local_state_->GetBoolean(prefs::kEnrollmentIdUploadedOnChromad);
}

void ActiveDirectoryDeviceStateUploader::CreateClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!cloud_policy_client_);
  cloud_policy_client_ = std::make_unique<CloudPolicyClient>(
      dm_service_, url_loader_factory_,
      CloudPolicyClient::DeviceDMTokenCallback());
}

void ActiveDirectoryDeviceStateUploader::OnStateKeysUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  state_keys_to_upload_ = state_keys_broker_->state_keys();
  if (state_keys_to_upload_.empty()) {
    LOG(ERROR) << "Empty state keys!";
    MaybeRunStatusCallback(std::move(state_keys_callback_for_testing_),
                           /*success=*/false);
    return;
  }

  CHECK(cloud_policy_client_);
  if (cloud_policy_client_->is_registered()) {
    FetchPolicyToUploadStateKeys();
    return;
  }

  if (dm_token_.empty()) {
    LOG(WARNING) << "State keys updated while DM Token was not available";
    return;
  }

  // Re-register client with previously stored DM Token if it was unregistered
  // for some reason. Unlikely to happen, but needs to be covered.
  RegisterClient();
}

void ActiveDirectoryDeviceStateUploader::OnDMTokenAvailable(
    const std::string& dm_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (dm_token.empty()) {
    LOG(ERROR) << "Retrieving the DM Token failed";
    MaybeRunStatusCallback(std::move(state_keys_callback_for_testing_),
                           /*success=*/false);
    MaybeRunStatusCallback(std::move(enrollment_id_callback_for_testing_),
                           /*success=*/false);
    return;
  }
  dm_token_ = dm_token;

  // Once we have non-empty DM Token, we don't need storage object anymore.
  dm_token_storage_.reset();

  RegisterClient();
}

void ActiveDirectoryDeviceStateUploader::RegisterClient() {
  DCHECK(!cloud_policy_client_->is_registered());
  cloud_policy_client_->SetupRegistration(
      dm_token_, client_id_,
      /*user_affiliation_ids=*/std::vector<std::string>());
}

void ActiveDirectoryDeviceStateUploader::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!client->is_registered()) {
    MaybeRunStatusCallback(std::move(state_keys_callback_for_testing_),
                           /*success=*/false);
    MaybeRunStatusCallback(std::move(enrollment_id_callback_for_testing_),
                           /*success=*/false);
    return;
  }

  if (!state_keys_to_upload_.empty()) {
    FetchPolicyToUploadStateKeys();
  }

  if (HasUploadedEnrollmentId()) {
    VLOG(1) << "No need to upload the enrollment ID: already uploaded";
    MaybeRunStatusCallback(std::move(enrollment_id_callback_for_testing_),
                           /*success=*/false);
    return;
  }

  UploadEnrollmentId();
}

void ActiveDirectoryDeviceStateUploader::FetchPolicyToUploadStateKeys() {
  DCHECK(!dm_token_.empty());
  DCHECK(!state_keys_to_upload_.empty());
  DCHECK(cloud_policy_client_->is_registered());
  VLOG(1) << "Fetching device cloud policies to upload state keys";

  // TODO(b/181140445): Introduce state keys upload request to DMServer and
  // separate it from policy fetching mechanism.

  cloud_policy_client_->SetStateKeysToUpload(state_keys_to_upload_);
  cloud_policy_client_->FetchPolicy();
}

void ActiveDirectoryDeviceStateUploader::UploadEnrollmentId() {
  DCHECK(cloud_policy_client_->is_registered());
  VLOG(1) << "Uploading Enrollment ID to DMServer";

  CHECK(enrollment_id_upload_manager_);
  enrollment_id_upload_manager_->ObtainAndUploadEnrollmentId(base::BindOnce(
      &ActiveDirectoryDeviceStateUploader::OnEnrollmentIdUploaded,
      weak_ptr_factory_.GetWeakPtr()));
}

void ActiveDirectoryDeviceStateUploader::OnEnrollmentIdUploaded(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (success) {
    VLOG(1) << "Enrollment ID successfully uploaded to DMServer";
    local_state_->SetBoolean(prefs::kEnrollmentIdUploadedOnChromad,
                             /*value=*/true);
  } else {
    LOG(ERROR) << "Failed to upload Enrollment ID to DMServer";
  }

  MaybeRunStatusCallback(std::move(enrollment_id_callback_for_testing_),
                         success);
}

void ActiveDirectoryDeviceStateUploader::OnPolicyFetched(
    CloudPolicyClient* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(1) << "Cloud policy fetched. Status = " << client->last_dm_status();

  bool success = (client->last_dm_status() == DM_STATUS_SUCCESS);
  if (!success) {
    LOG(ERROR) << "Failed to fetch policy to upload state keys";
  }

  MaybeRunStatusCallback(std::move(state_keys_callback_for_testing_), success);
}

void ActiveDirectoryDeviceStateUploader::OnClientError(
    CloudPolicyClient* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(ERROR) << "Failed to upload state keys. Status = "
             << client->last_dm_status();

  MaybeRunStatusCallback(std::move(state_keys_callback_for_testing_),
                         /*success=*/false);
}

}  // namespace policy

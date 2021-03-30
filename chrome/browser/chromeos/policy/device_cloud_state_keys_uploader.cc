// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_state_keys_uploader.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "chrome/browser/chromeos/policy/dm_token_storage.h"
#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/dbus/login_manager/policy_descriptor.pb.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

namespace policy {

DeviceCloudStateKeysUploader::DeviceCloudStateKeysUploader(
    const std::string& client_id,
    DeviceManagementService* dm_service,
    ServerBackedStateKeysBroker* state_keys_broker,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<DMTokenStorageBase> dm_token_storage)
    : client_id_(client_id),
      dm_service_(dm_service),
      state_keys_broker_(state_keys_broker),
      url_loader_factory_(url_loader_factory),
      dm_token_storage_(std::move(dm_token_storage)) {}

DeviceCloudStateKeysUploader::~DeviceCloudStateKeysUploader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cloud_policy_client_) {
    Shutdown();
  }
}

void DeviceCloudStateKeysUploader::SetStatusCallbackForTesting(
    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  status_callback_for_testing_ = std::move(callback);
}

bool DeviceCloudStateKeysUploader::IsClientRegistered() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(cloud_policy_client_);
  return cloud_policy_client_->is_registered();
}

void DeviceCloudStateKeysUploader::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!cloud_policy_client_);
  cloud_policy_client_ = std::make_unique<CloudPolicyClient>(
      dm_service_, url_loader_factory_,
      CloudPolicyClient::DeviceDMTokenCallback());
  cloud_policy_client_->AddObserver(this);
  cloud_policy_client_->AddPolicyTypeToFetch(
      dm_protocol::kChromeDevicePolicyType,
      /*settings_entity_id=*/std::string());

  DCHECK(!state_keys_update_subscription_);
  state_keys_update_subscription_ = state_keys_broker_->RegisterUpdateCallback(
      base::BindRepeating(&DeviceCloudStateKeysUploader::OnStateKeysUpdated,
                          weak_ptr_factory_.GetWeakPtr()));
}

void DeviceCloudStateKeysUploader::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(cloud_policy_client_);
  cloud_policy_client_->RemoveObserver(this);
  cloud_policy_client_.reset();
  state_keys_update_subscription_ = {};
}

void DeviceCloudStateKeysUploader::OnStateKeysUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  state_keys_to_upload_ = state_keys_broker_->state_keys();
  if (state_keys_to_upload_.empty()) {
    LOG(ERROR) << "Empty state keys!";
    if (status_callback_for_testing_) {
      std::move(status_callback_for_testing_).Run(/*success=*/false);
    }
    return;
  }

  CHECK(cloud_policy_client_);
  if (!cloud_policy_client_->is_registered()) {
    if (!dm_token_.empty()) {
      // Re-register client with previously stored DM Token if it was
      // unregistered for some reason.
      RegisterClient();
    } else if (!dm_token_requested_) {
      // Request DM Token before uploading state keys.
      // Make sure only one request is made in case multiple state key updates
      // happen before first request is completed. This is not expected to
      // happen, because state keys polling is very slow (once per day), but
      // needs to be handled for correctness.
      dm_token_requested_ = true;
      CHECK(dm_token_storage_);
      dm_token_storage_->RetrieveDMToken(
          base::BindOnce(&DeviceCloudStateKeysUploader::OnDMTokenAvailable,
                         weak_ptr_factory_.GetWeakPtr()));
    } else {
      LOG(WARNING) << "State keys updated during DM Token retrieval";
    }
  } else {
    FetchPolicyToUploadStateKeys();
  }
}

void DeviceCloudStateKeysUploader::OnDMTokenAvailable(
    const std::string& dm_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (dm_token.empty()) {
    LOG(ERROR) << "Retrieving the DMToken failed";
    if (status_callback_for_testing_) {
      std::move(status_callback_for_testing_).Run(/*success=*/false);
    }
    return;
  }
  dm_token_ = dm_token;

  // Once we have non-empty DM Token, we don't need storage object anymore.
  dm_token_storage_.reset();

  RegisterClient();
}

void DeviceCloudStateKeysUploader::RegisterClient() {
  DCHECK(!cloud_policy_client_->is_registered());
  cloud_policy_client_->SetupRegistration(
      dm_token_, client_id_,
      /*user_affiliation_ids=*/std::vector<std::string>());
}

void DeviceCloudStateKeysUploader::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (client->is_registered()) {
    FetchPolicyToUploadStateKeys();
  }
}

void DeviceCloudStateKeysUploader::FetchPolicyToUploadStateKeys() {
  DCHECK(!dm_token_.empty());
  DCHECK(!state_keys_to_upload_.empty());
  DCHECK(cloud_policy_client_->is_registered());
  VLOG(1) << "Fetching device cloud policies to upload state keys";

  // TODO(b/181140445): Introduce state keys upload request to DM Server and
  // separate it from policy fetching mechanism.

  cloud_policy_client_->SetStateKeysToUpload(state_keys_to_upload_);
  cloud_policy_client_->FetchPolicy();
}

void DeviceCloudStateKeysUploader::OnPolicyFetched(CloudPolicyClient* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(1) << "Cloud policy fetched. Status = " << client->status();

  bool success = (client->status() == DM_STATUS_SUCCESS);
  if (!success) {
    LOG(ERROR) << "Failed to fetch policy to upload state keys";
  }

  if (status_callback_for_testing_) {
    std::move(status_callback_for_testing_).Run(success);
  }
}

void DeviceCloudStateKeysUploader::OnClientError(CloudPolicyClient* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(ERROR) << "Failed to upload state keys. Status = " << client->status();

  if (status_callback_for_testing_) {
    std::move(status_callback_for_testing_).Run(/*success=*/false);
  }
}

}  // namespace policy

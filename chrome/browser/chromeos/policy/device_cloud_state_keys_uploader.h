// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_STATE_KEYS_UPLOADER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_STATE_KEYS_UPLOADER_H_

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace policy {

class DMTokenStorageBase;
class DeviceManagementService;
class ServerBackedStateKeysBroker;

// Uploads state keys to DMServer for Active Directory mode.
class DeviceCloudStateKeysUploader : public CloudPolicyClient::Observer {
 public:
  DeviceCloudStateKeysUploader(
      const std::string& client_id,
      DeviceManagementService* dm_service,
      ServerBackedStateKeysBroker* state_keys_broker,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<DMTokenStorageBase> dm_token_storage);

  ~DeviceCloudStateKeysUploader() override;

  // Subscribes to state keys update signal to trigger state keys upload
  // whenever state keys are updated.
  void Init();

  // Unsubscribes from state keys update signal.
  void Shutdown();

  // Callback after state keys uploading sequence completed successfully or
  // failed.
  using StatusCallback = base::OnceCallback<void(bool success)>;

  // Sets state keys uploading sequence status callback for testing.
  void SetStatusCallbackForTesting(StatusCallback callback);

  // Returns cloud policy client registration state. This should be only used in
  // tests.
  bool IsClientRegistered() const;

  // Returns state keys to upload. This should be only used in tests.
  const std::vector<std::string>& state_keys_to_upload() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return state_keys_to_upload_;
  }

 private:
  // Callback when state keys are updated.
  void OnStateKeysUpdated();

  // Callback when DM Token is available.
  void OnDMTokenAvailable(const std::string& dm_token);

  // Registers cloud policy client. After registration is complete, the policy
  // fetch to upload state keys is triggered.
  void RegisterClient();

  // Cloud policy fetch to upload state keys.
  void FetchPolicyToUploadStateKeys();

  // CloudPolicyClient::Observer:
  void OnRegistrationStateChanged(CloudPolicyClient* client) override;
  void OnPolicyFetched(CloudPolicyClient* client) override;
  void OnClientError(CloudPolicyClient* client) override;

  // Assert non-concurrent usage in debug builds.
  SEQUENCE_CHECKER(sequence_checker_);

  std::string client_id_;

  DeviceManagementService* dm_service_;

  ServerBackedStateKeysBroker* state_keys_broker_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  std::unique_ptr<DMTokenStorageBase> dm_token_storage_;

  std::string dm_token_;

  // Set to true if DM Token was requested.
  bool dm_token_requested_ = false;

  std::unique_ptr<CloudPolicyClient> cloud_policy_client_;

  base::CallbackListSubscription state_keys_update_subscription_;

  // List of state keys to upload by attaching to DM policy fetch request.
  std::vector<std::string> state_keys_to_upload_;

  StatusCallback status_callback_for_testing_;

  // Must be last member.
  base::WeakPtrFactory<DeviceCloudStateKeysUploader> weak_ptr_factory_{this};

  // Disallow copying.
  DeviceCloudStateKeysUploader(const DeviceCloudStateKeysUploader&) = delete;
  DeviceCloudStateKeysUploader& operator=(const DeviceCloudStateKeysUploader&) =
      delete;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_STATE_KEYS_UPLOADER_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SERVER_BACKED_STATE_ACTIVE_DIRECTORY_DEVICE_STATE_UPLOADER_H_
#define CHROME_BROWSER_ASH_POLICY_SERVER_BACKED_STATE_ACTIVE_DIRECTORY_DEVICE_STATE_UPLOADER_H_

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {
class DeviceSettingsService;

namespace attestation {
class EnrollmentCertificateUploader;
class EnrollmentCertificateUploaderImpl;
class EnrollmentIdUploadManager;
}  // namespace attestation
}  // namespace ash

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace policy {

class DMTokenStorageBase;
class DeviceManagementService;
class ServerBackedStateKeysBroker;

// Uploads state keys and the enrollment ID to DMServer for Active Directory
// mode.
class ActiveDirectoryDeviceStateUploader : public CloudPolicyClient::Observer {
 public:
  ActiveDirectoryDeviceStateUploader(
      const std::string& client_id,
      DeviceManagementService* dm_service,
      ServerBackedStateKeysBroker* state_keys_broker,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<DMTokenStorageBase> dm_token_storage,
      PrefService* local_state);

  ~ActiveDirectoryDeviceStateUploader() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Returns true if the enrollment ID has already been uploaded.
  bool HasUploadedEnrollmentId() const;

  // Subscribes to state keys update signal to trigger state keys upload
  // whenever state keys are updated. Also, starts a DM Token request.
  void Init();

  // Unsubscribes from state keys update signal.
  void Shutdown();

  // Callback called when one of the following sequences completes successfully
  // of fails: state keys upload or enrollment ID upload.
  using StatusCallback = base::OnceCallback<void(bool success)>;

  // Sets state keys uploading sequence status callback for testing.
  void SetStateKeysCallbackForTesting(StatusCallback callback);

  // Sets enrollemt ID uploading sequence status callback for testing.
  void SetEnrollmentIdCallbackForTesting(StatusCallback callback);

  // Sets a custom device settings service that should be used for testing.
  void SetDeviceSettingsServiceForTesting(
      ash::DeviceSettingsService* device_settings_service);

  // Sets the value of kEnrollmentIdUploadedOnChromad pref. Used in tests.
  void SetEnrollmentIdUploadedForTesting(bool value);

  // Sets a custom certificate uploader that should be used for testing.
  void SetEnrollmentCertificateUploaderForTesting(
      std::unique_ptr<ash::attestation::EnrollmentCertificateUploaderImpl>
          enrollment_certificate_uploader_impl);

  // Allows instantiating the `cloud_policy_client_` before calling `Init()`.
  // Returns the recently created `CloudPolicyClient`. Used in tests.
  CloudPolicyClient* CreateClientForTesting();

  // Returns cloud policy client registration state. Used in tests.
  bool IsClientRegisteredForTesting() const;

  // Returns state keys to upload. This should be only used in tests.
  const std::vector<std::string>& state_keys_to_upload_for_testing() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return state_keys_to_upload_;
  }

 private:
  // Instantiates the `cloud_policy_client_`.
  void CreateClient();

  // Callback when state keys are updated.
  void OnStateKeysUpdated();

  // Callback when DM Token is available.
  void OnDMTokenAvailable(const std::string& dm_token);

  // Registers cloud policy client. After registration is complete, the policy
  // fetch to upload state keys is triggered. The enrollment ID will also be
  // uploaded, if it hasn't been yet.
  void RegisterClient();

  // Cloud policy fetch to upload state keys.
  void FetchPolicyToUploadStateKeys();

  // Uploads the enrollment ID to DMServer.
  void UploadEnrollmentId();

  // CloudPolicyClient::Observer:
  void OnRegistrationStateChanged(CloudPolicyClient* client) override;
  void OnPolicyFetched(CloudPolicyClient* client) override;
  void OnClientError(CloudPolicyClient* client) override;

  // Logs the result, and updates prefs if the upload succeeded.
  void OnEnrollmentIdUploaded(bool success);

  // Assert non-concurrent usage in debug builds.
  SEQUENCE_CHECKER(sequence_checker_);

  std::string client_id_;

  DeviceManagementService* dm_service_;

  ServerBackedStateKeysBroker* state_keys_broker_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  std::unique_ptr<DMTokenStorageBase> dm_token_storage_;

  std::string dm_token_;

  // Local state prefs, not owned.
  PrefService* local_state_ = nullptr;

  std::unique_ptr<CloudPolicyClient> cloud_policy_client_;

  base::CallbackListSubscription state_keys_update_subscription_;

  // List of state keys to upload by attaching to DM policy fetch request.
  std::vector<std::string> state_keys_to_upload_;

  std::unique_ptr<ash::attestation::EnrollmentCertificateUploader>
      enrollment_certificate_uploader_;
  std::unique_ptr<ash::attestation::EnrollmentIdUploadManager>
      enrollment_id_upload_manager_;

  StatusCallback state_keys_callback_for_testing_;

  StatusCallback enrollment_id_callback_for_testing_;

  // Not owned.
  ash::DeviceSettingsService* device_settings_service_for_testing_ = nullptr;

  // Must be last member.
  base::WeakPtrFactory<ActiveDirectoryDeviceStateUploader> weak_ptr_factory_{
      this};

  // Disallow copying.
  ActiveDirectoryDeviceStateUploader(
      const ActiveDirectoryDeviceStateUploader&) = delete;
  ActiveDirectoryDeviceStateUploader& operator=(
      const ActiveDirectoryDeviceStateUploader&) = delete;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_SERVER_BACKED_STATE_ACTIVE_DIRECTORY_DEVICE_STATE_UPLOADER_H_

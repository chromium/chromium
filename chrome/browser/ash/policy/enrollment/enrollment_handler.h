// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ENROLLMENT_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ENROLLMENT_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_validator.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/policy/device_account_initializer.h"
#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "google_apis/gaia/gaia_oauth_client.h"

namespace ash {
namespace attestation {
class AttestationFeatures;
}  // namespace attestation
}  // namespace ash

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace policy {

class DeviceCloudPolicyStoreAsh;
class EnrollmentStatus;
class ServerBackedStateKeysBroker;
class SigningService;

// Implements the logic that establishes enterprise enrollment for Chromium OS
// devices. The process is as follows:
//   1. Given an auth token, register with the policy service.
//   2. Download the initial policy blob from the service.
//   3. Verify the policy blob. Everything up to this point doesn't touch device
//      state.
//   4. Download the OAuth2 authorization code for device-level API access.
//   5. Download the OAuth2 refresh token for device-level API access and store
//      it.
//   6. Establish the device lock in installation-time attributes.
//   7. Store the policy blob and API refresh token.
class EnrollmentHandler : public CloudPolicyClient::Observer,
                          public CloudPolicyStore::Observer,
                          public DeviceAccountInitializer::Delegate {
 public:
  using EnrollmentCallback = base::OnceCallback<void(EnrollmentStatus)>;

  // Base class for factories providing SigningService. Exists for testing.
  class SigningServiceProvider {
   public:
    virtual ~SigningServiceProvider() = default;

    virtual std::unique_ptr<SigningService> CreateSigningService() const = 0;
  };

  // |store| and |install_attributes| must remain valid for the life time of the
  // enrollment handler.
  EnrollmentHandler(
      DeviceCloudPolicyStoreAsh* store,
      ash::InstallAttributes* install_attributes,
      ServerBackedStateKeysBroker* state_keys_broker,
      ash::attestation::AttestationFlow* attestation_flow,
      std::unique_ptr<CloudPolicyClient> client,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const EnrollmentConfig& enrollment_config,
      DMAuth dm_auth,
      const std::string& client_id,
      const std::string& requisition,
      const std::string& sub_organization,
      EnrollmentCallback completion_callback);

  EnrollmentHandler(const EnrollmentHandler&) = delete;
  EnrollmentHandler& operator=(const EnrollmentHandler&) = delete;

  ~EnrollmentHandler() override;

  void SetSigningServiceProviderForTesting(
      std::unique_ptr<SigningServiceProvider> signing_service_provider);

  // Starts the enrollment process and reports the result to
  // |completion_callback_|.
  void StartEnrollment();

  // Releases the client.
  std::unique_ptr<CloudPolicyClient> ReleaseClient();

  // CloudPolicyClient::Observer:
  void OnPolicyFetched(CloudPolicyClient* client) override;
  void OnRegistrationStateChanged(CloudPolicyClient* client) override;
  void OnClientError(CloudPolicyClient* client) override;

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

  // DeviceAccountInitializer::Delegate:
  void OnDeviceAccountTokenFetched(bool empty_token) override;
  void OnDeviceAccountTokenStored() override;
  void OnDeviceAccountTokenFetchError(
      std::optional<DeviceManagementStatus> dm_status) override;
  void OnDeviceAccountTokenStoreError() override;
  void OnDeviceAccountClientError(DeviceManagementStatus status) override;
  enterprise_management::DeviceServiceApiAccessRequest::DeviceType
  GetRobotAuthCodeDeviceType() override;
  std::set<std::string> GetRobotOAuthScopes() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

 private:
  // Indicates what step of the process is currently pending. These steps need
  // to be listed in the order they are traversed in.  (Steps are numbered
  // explicitly to make it easier to read debug logs.)
  enum EnrollmentStep {
    STEP_PENDING = 0,           // Not started yet.
    STEP_STATE_KEYS = 1,        // Waiting for state keys to become available.
    STEP_LOADING_STORE = 2,     // Waiting for |store_| to initialize.
    STEP_REGISTRATION = 3,      // Currently registering the client.
    STEP_POLICY_FETCH = 4,      // Fetching policy.
    STEP_VALIDATION = 5,        // Policy validation.
    STEP_ROBOT_AUTH_FETCH = 6,  // Fetching device API auth code.
    UNUSED_ROBOT_AUTH_REFRESH = 7,  // Fetching device API refresh token.
    UNUSED_AD_DOMAIN_JOIN = 8,      // Joining Active Directory domain.
    STEP_SET_FWMP_DATA = 9,      // Setting the firmware management parameters.
    STEP_LOCK_DEVICE = 10,       // Writing installation-time attributes.
    STEP_STORE_TOKEN = 11,       // Encrypting and storing DM token.
    STEP_STORE_ROBOT_AUTH = 12,  // Encrypting & writing robot refresh token.
    STEP_STORE_VERSION = 13,     // Storing OS and browser version.
    STEP_STORE_POLICY = 14,      // Storing policy and API refresh token.
    STEP_FINISHED = 15,          // Enrollment process done, no further action.
  };

  // Handles state keys, present or not.
  void HandleStateKeys(std::optional<std::vector<std::string>> opt_state_keys);

  // Starts attestation based enrollment flow.
  void StartAttestationBasedEnrollmentFlow();

  // Checks the Attestation Features and gets a fresh certificate.
  void OnGetFeaturesReady(
      ash::attestation::AttestationFlow::CertificateCallback callback,
      const ash::attestation::AttestationFeatures* features);

  // Handles the response to a request for a registration certificate.
  void HandleRegistrationCertificateResult(
      ash::attestation::AttestationStatus status,
      const std::string& pem_certificate_chain);

  // Starts registration if the store is initialized.
  void StartRegistration();

  // Handles the policy validation result, proceeding with device lock if
  // successful.
  void HandlePolicyValidationResult(DeviceCloudPolicyValidator* validator);

  // Updates the firmware management partition from TPM, setting the flags
  // according to enum FirmwareManagementParametersFlags from rpc.proto if
  // devmode is blocked.
  void SetFirmwareManagementParametersData();

  // Invoked after the firmware management partition in TPM is updated.
  void OnFirmwareManagementParametersDataSet(
      std::optional<device_management::SetFirmwareManagementParametersReply>
          reply);

  // Calls InstallAttributes::LockDevice() for enterprise enrollment and
  // DeviceSettingsService::SetManagementSettings() for consumer
  // enrollment.
  void StartLockDevice();

  // Handle callback from InstallAttributes::LockDevice() and retry on failure.
  void HandleLockDeviceResult(ash::InstallAttributes::LockResult lock_result);

  // Initiates storing of robot auth token.
  void StartStoreRobotAuth();

  // Store the version related information.
  void StoreVersion();

  // Store the device policy.
  void StartStoreDevicePolicy();

  std::unique_ptr<DeviceCloudPolicyValidator> CreateValidator(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy,
      const std::string& domain);

  // Drops any ongoing actions.
  void Stop();

  // Reports the result of the enrollment process to the initiator.
  void ReportResult(EnrollmentStatus status);

  // Set |enrollment_step_| to |step|.
  void SetStep(EnrollmentStep step);

  raw_ptr<DeviceCloudPolicyStoreAsh> store_;
  raw_ptr<ash::InstallAttributes> install_attributes_;
  raw_ptr<ServerBackedStateKeysBroker> state_keys_broker_;
  raw_ptr<ash::attestation::AttestationFlow> attestation_flow_;
  // Factory for SigningService to be used by |client_| to register with.
  std::unique_ptr<SigningServiceProvider> signing_service_provider_;
  std::unique_ptr<CloudPolicyClient> client_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  std::unique_ptr<DeviceAccountInitializer> device_account_initializer_;

  EnrollmentConfig enrollment_config_;
  DMAuth dm_auth_;
  std::string client_id_;
  std::string sub_organization_;
  std::unique_ptr<CloudPolicyClient::RegistrationParameters> register_params_;
  EnrollmentCallback completion_callback_;

  // The device mode as received in the registration request.
  DeviceMode device_mode_ = DEVICE_MODE_NOT_SET;

  // Whether the server signaled to skip robot auth setup.
  bool skip_robot_auth_ = false;

  // The validated policy response info to be installed in the store.
  std::unique_ptr<enterprise_management::PolicyFetchResponse> policy_;
  std::string domain_;
  std::string realm_;
  std::string device_id_;

  // Current enrollment step.
  EnrollmentStep enrollment_step_;

  // Total amount of time in milliseconds spent waiting for lockbox
  // initialization.
  int lockbox_init_duration_ = 0;

  base::WeakPtrFactory<EnrollmentHandler> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ENROLLMENT_HANDLER_H_

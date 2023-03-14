// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ENROLLMENT_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ENROLLMENT_HANDLER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_validator.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/policy/device_account_initializer.h"
#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "chromeos/ash/components/dbus/authpolicy/authpolicy_client.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace attestation {
class AttestationFeatures;
}  // namespace attestation
}  // namespace ash

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace policy {

class ActiveDirectoryJoinDelegate;
class DeviceCloudPolicyStoreAsh;
class DMTokenStorage;
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
      ActiveDirectoryJoinDelegate* ad_join_delegate,
      const EnrollmentConfig& enrollment_config,
      LicenseType license_type,
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
      absl::optional<DeviceManagementStatus> dm_status) override;
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
    STEP_AD_DOMAIN_JOIN = 8,        // Joining Active Directory domain.
    STEP_SET_FWMP_DATA = 9,      // Setting the firmware management parameters.
    STEP_LOCK_DEVICE = 10,       // Writing installation-time attributes.
    STEP_STORE_TOKEN = 11,       // Encrypting and storing DM token.
    STEP_STORE_ROBOT_AUTH = 12,  // Encrypting & writing robot refresh token.
    STEP_STORE_POLICY = 13,      // Storing policy and API refresh token. For
                                 // AD, includes policy fetch via authpolicyd.
    STEP_FINISHED = 14,          // Enrollment process done, no further action.
  };

  // Handles the response to a request for server-backed state keys.
  void HandleStateKeysResult(const std::vector<std::string>& state_keys);

  // Starts attestation based enrollment flow. If |is_initial_attempt| is true,
  // uses existing certificate if any. Otherwise, uses a new certificate.
  void StartAttestationBasedEnrollmentFlow(bool is_initial_attempt);

  // Checks the Attestation Features and gets a suitable certificate.
  void OnGetFeaturesReady(
      bool force_new_key,
      ash::attestation::AttestationFlow::CertificateCallback callback,
      const ash::attestation::AttestationFeatures* features);

  // Handles the response to a request for a registration certificate.
  // |is_initial_attempt| indicates whether it is the first attempt to obtain
  // valid enrollment certificate. If |is_initial_attempt| is true, then
  // |StartAttestationBasedEnrollmentFlow| attempted to fetch existing
  // certificate if any. Otherwise, it attempted to fetch a fresh certificate.
  void HandleRegistrationCertificateResult(
      bool is_initial_attempt,
      ash::attestation::AttestationStatus status,
      const std::string& pem_certificate_chain);

  // Starts registration if the store is initialized.
  void StartRegistration();

  // Handles the policy validation result, proceeding with device lock if
  // successful.
  void HandlePolicyValidationResult(DeviceCloudPolicyValidator* validator);

  // Start joining the Active Directory domain in case the device is enrolling
  // into Active Directory management mode.
  void StartJoinAdDomain();

  // Handles successful Active Directory domain join.
  void OnAdDomainJoined(const std::string& realm);

  // Updates the firmware management partition from TPM, setting the flags
  // according to enum FirmwareManagementParametersFlags from rpc.proto if
  // devmode is blocked.
  void SetFirmwareManagementParametersData();

  // Invoked after the firmware management partition in TPM is updated.
  void OnFirmwareManagementParametersDataSet(
      absl::optional<user_data_auth::SetFirmwareManagementParametersReply>
          reply);

  // Calls InstallAttributes::LockDevice() for enterprise enrollment and
  // DeviceSettingsService::SetManagementSettings() for consumer
  // enrollment.
  void StartLockDevice();

  // Handle callback from InstallAttributes::LockDevice() and retry on failure.
  void HandleLockDeviceResult(ash::InstallAttributes::LockResult lock_result);

  // Initiates storing DM token. For Active Directory devices only.
  void StartStoreDMToken();

  // Called after StartStoreDMtoken() is done.
  void HandleDMTokenStoreResult(bool success);

  // Initiates storing of robot auth token.
  void StartStoreRobotAuth();

  // Handles result from device policy refresh via authpolicyd.
  void HandleActiveDirectoryPolicyRefreshed(authpolicy::ErrorType error);

  std::unique_ptr<DeviceCloudPolicyValidator> CreateValidator(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy,
      const std::string& domain);

  // Drops any ongoing actions.
  void Stop();

  // Reports the result of the enrollment process to the initiator.
  void ReportResult(EnrollmentStatus status);

  // Set |enrollment_step_| to |step|.
  void SetStep(EnrollmentStep step);

  DeviceCloudPolicyStoreAsh* store_;
  ash::InstallAttributes* install_attributes_;
  ServerBackedStateKeysBroker* state_keys_broker_;
  ash::attestation::AttestationFlow* attestation_flow_;
  // Factory for SigningService to be used by |client_| to register with.
  std::unique_ptr<SigningServiceProvider> signing_service_provider_;
  std::unique_ptr<CloudPolicyClient> client_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  ActiveDirectoryJoinDelegate* ad_join_delegate_ = nullptr;
  std::unique_ptr<DeviceAccountInitializer> device_account_initializer_;
  std::unique_ptr<DMTokenStorage> dm_token_storage_;

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

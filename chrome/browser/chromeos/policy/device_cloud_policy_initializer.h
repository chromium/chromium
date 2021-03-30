// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_POLICY_INITIALIZER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_POLICY_INITIALIZER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"
#include "chromeos/dbus/attestation/interface.pb.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/signing_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class PrefService;

namespace base {
class SequencedTaskRunner;
}

namespace chromeos {

class ActiveDirectoryJoinDelegate;
class InstallAttributes;

namespace attestation {
class AttestationFlow;
}
namespace system {
class StatisticsProvider;
}

}  // namespace chromeos

namespace policy {

class DeviceCloudPolicyManagerChromeOS;
class DeviceCloudPolicyStoreChromeOS;
class DeviceManagementService;
struct EnrollmentConfig;
class EnrollmentHandlerChromeOS;
class EnrollmentStatus;

// This class connects DCPM to the correct device management service, and
// handles the enrollment process.
class DeviceCloudPolicyInitializer : public CloudPolicyStore::Observer {
 public:
  using EnrollmentCallback = base::OnceCallback<void(EnrollmentStatus)>;

  // |background_task_runner| is used to execute long-running background tasks
  // that may involve file I/O.
  DeviceCloudPolicyInitializer(
      PrefService* local_state,
      DeviceManagementService* enterprise_service,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      chromeos::InstallAttributes* install_attributes,
      ServerBackedStateKeysBroker* state_keys_broker,
      DeviceCloudPolicyStoreChromeOS* policy_store,
      DeviceCloudPolicyManagerChromeOS* policy_manager,
      std::unique_ptr<chromeos::attestation::AttestationFlow> attestation_flow,
      chromeos::system::StatisticsProvider* statistics_provider);

  ~DeviceCloudPolicyInitializer() override;

  virtual void Init();
  virtual void Shutdown();

  // Prepares enrollment or re-enrollment. Enrollment should be started by
  // calling StartEnrollment. Once the enrollment process
  // completes, |enrollment_callback| is invoked and gets passed the status of
  // the operation.
  virtual void PrepareEnrollment(
      DeviceManagementService* device_management_service,
      chromeos::ActiveDirectoryJoinDelegate* ad_join_delegate,
      const EnrollmentConfig& enrollment_config,
      DMAuth dm_auth,
      EnrollmentCallback enrollment_callback);

  // Starts enrollment.
  virtual void StartEnrollment();

  // Get the enrollment configuration that has been set up via signals such as
  // device requisition, OEM manifest, pre-existing installation-time attributes
  // or server-backed state retrieval. The configuration is stored in |config|,
  // |config.mode| will be MODE_NONE if there is no prescribed configuration.
  // |config.management_domain| will contain the domain the device is supposed
  // to be enrolled to as decided by factors such as forced re-enrollment,
  // enrollment recovery, or already-present install attributes. Note that
  // |config.management_domain| may be non-empty even if |config.mode| is
  // MODE_NONE.
  EnrollmentConfig GetPrescribedEnrollmentConfig() const;

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

  // Allows testing code to set a signing service tailored to its needs.
  void SetSigningServiceForTesting(
      std::unique_ptr<policy::SigningService> signing_service);
  void SetSystemURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory);
  void SetAttestationFlowForTesting(
      std::unique_ptr<chromeos::attestation::AttestationFlow> attestation_flow);

 private:
  // Signing class implementing the policy::SigningService interface to
  // sign data using the enrollment certificate's TPM-bound key.
  class TpmEnrollmentKeySigningService : public policy::SigningService {
   public:
    TpmEnrollmentKeySigningService();
    ~TpmEnrollmentKeySigningService() override;

    void SignData(const std::string& data, SigningCallback callback) override;

   private:
    void OnDataSigned(const std::string& data,
                      SigningCallback callback,
                      const ::attestation::SignSimpleChallengeReply& reply);

    // Used to create tasks which run delayed on the UI thread.
    base::WeakPtrFactory<TpmEnrollmentKeySigningService> weak_ptr_factory_{
        this};
  };

  // TODO(crbug.com/705758) When DeviceCloudPolicyInitializer starts connection,
  // that means it will be deleted soon by
  // |BrowserPolicyConnectorChromeOS::OnDeviceCloudPolicyManagerConnected|.
  // Sometimes this happens before |EnterpriseEnrollmentHelperImpl::DoEnroll|
  // initiates |StartConnection| and leads to a crash. Track the reason of
  // |StartConnection| call to find who initiates removal. Remove once the crash
  // is resolved.
  enum class StartConnectionReason {
    // |StartConnection| succeeds after call from |Init|.
    kInitialCreation = 0,
    // |StartConnection| succeeds after notification from |state_keys_broker_|.
    kStateKeysStored = 1,
    // |StartConnection| succeeds after notification from |policy_store_|.
    kCloudPolicyLoaded = 2,
    // |StartConnection| succeeds after call from |StartEnrollment|.
    kEnrollmentCompleted = 3,
  };

  FRIEND_TEST_ALL_PREFIXES(
      DeviceCloudPolicyInitializerTpmEnrollmentKeySigningServiceTest,
      SigningSuccess);
  FRIEND_TEST_ALL_PREFIXES(
      DeviceCloudPolicyInitializerTpmEnrollmentKeySigningServiceTest,
      SigningFailure);

  // Handles completion signaled by |enrollment_handler_|.
  void EnrollmentCompleted(EnrollmentCallback enrollment_callback,
                           EnrollmentStatus status);

  // Creates a new CloudPolicyClient.
  std::unique_ptr<CloudPolicyClient> CreateClient(
      DeviceManagementService* device_management_service);

  void TryToCreateClient(StartConnectionReason reason);
  void StartConnection(StartConnectionReason reason,
                       std::unique_ptr<CloudPolicyClient> client);

  // Get a machine flag from |statistics_provider_|, returning the given
  // |default_value| if not present.
  bool GetMachineFlag(const std::string& key, bool default_value) const;

  PrefService* local_state_;
  DeviceManagementService* enterprise_service_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  chromeos::InstallAttributes* install_attributes_;
  ServerBackedStateKeysBroker* state_keys_broker_;
  DeviceCloudPolicyStoreChromeOS* policy_store_;
  DeviceCloudPolicyManagerChromeOS* policy_manager_;
  std::unique_ptr<chromeos::attestation::AttestationFlow> attestation_flow_;
  chromeos::system::StatisticsProvider* statistics_provider_;
  bool is_initialized_ = false;

  // Non-NULL if there is an enrollment operation pending.
  std::unique_ptr<EnrollmentHandlerChromeOS> enrollment_handler_;

  base::CallbackListSubscription state_keys_update_subscription_;

  // Our signing service.
  std::unique_ptr<SigningService> signing_service_;

  // The URLLoaderFactory set in tests.
  scoped_refptr<network::SharedURLLoaderFactory>
      system_url_loader_factory_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(DeviceCloudPolicyInitializer);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_POLICY_INITIALIZER_H_

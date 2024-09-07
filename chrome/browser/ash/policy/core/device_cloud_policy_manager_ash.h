// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_CLOUD_POLICY_MANAGER_ASH_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_CLOUD_POLICY_MANAGER_ASH_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/user_manager/user_manager.h"

namespace reporting {
class MetricReportingManager;
class OsUpdatesReporter;
class UserAddedRemovedReporter;
class UserEventReporterHelper;
}  // namespace reporting

namespace ash {
namespace attestation {
class EnrollmentCertificateUploader;
class EnrollmentIdUploadManager;
class MachineCertificateUploader;
}  // namespace attestation
namespace reporting {
class LoginLogoutReporter;
class LockUnlockReporter;
}  // namespace reporting
class InstallAttributes;
}  // namespace ash

namespace base {
class SequencedTaskRunner;
}  // namespace base

class PrefRegistrySimple;
class PrefService;

namespace policy {

class StartCrdSessionJobDelegate;
class DeviceCloudPolicyStoreAsh;
class EuiccStatusUploader;
class ForwardingSchemaRegistry;
class HeartbeatScheduler;
class LookupKeyUploader;
class ManagedSessionService;
class ReportingUserTracker;
class SchemaRegistry;
class StatusUploader;
class SystemLogUploader;

// CloudPolicyManager specialization for device policy in Ash.
class DeviceCloudPolicyManagerAsh : public CloudPolicyManager,
                                    public user_manager::UserManager::Observer {
 public:
  class Observer {
   public:
    // Invoked when the device cloud policy manager connects.
    virtual void OnDeviceCloudPolicyManagerConnected() = 0;
    // Invoked when the device cloud policy manager obtains schema registry.
    virtual void OnDeviceCloudPolicyManagerGotRegistry() = 0;
  };

  // |task_runner| is the runner for policy refresh, heartbeat, and status
  // upload tasks.
  DeviceCloudPolicyManagerAsh(
      std::unique_ptr<DeviceCloudPolicyStoreAsh> device_store,
      std::unique_ptr<CloudExternalDataManager> external_data_manager,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      ServerBackedStateKeysBroker* state_keys_broker,
      StartCrdSessionJobDelegate& crd_delegate);

  DeviceCloudPolicyManagerAsh(const DeviceCloudPolicyManagerAsh&) = delete;
  DeviceCloudPolicyManagerAsh& operator=(const DeviceCloudPolicyManagerAsh&) =
      delete;

  ~DeviceCloudPolicyManagerAsh() override;

  // ConfigurationPolicyProvider:
  void Init(SchemaRegistry* registry) override;

  // Initializes state keys.
  void Initialize(PrefService* local_state);

  void AddDeviceCloudPolicyManagerObserver(Observer* observer);
  void RemoveDeviceCloudPolicyManagerObserver(Observer* observer);

  // CloudPolicyManager:
  void Shutdown() override;

  // Pref registration helper.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Starts the connection via |client_to_connect|.
  void StartConnection(std::unique_ptr<CloudPolicyClient> client_to_connect,
                       ash::InstallAttributes* install_attributes);

  // Called when policy store is ready.
  void OnPolicyStoreReady(ash::InstallAttributes* install_attributes);

  bool IsConnected() const { return core()->service() != nullptr; }

  bool HasSchemaRegistry() const {
    return signin_profile_forwarding_schema_registry_ != nullptr;
  }

  DeviceCloudPolicyStoreAsh* device_store() { return device_store_.get(); }
  ReportingUserTracker* reporting_user_tracker() {
    return reporting_user_tracker_.get();
  }

  // Return the StatusUploader used to communicate device status to the
  // policy server.
  StatusUploader* GetStatusUploader() const { return status_uploader_.get(); }

  // Return the SystemLogUploader used to upload device logs to the policy
  // server.
  SystemLogUploader* GetSystemLogUploader() const {
    return syslog_uploader_.get();
  }

  // Passes the pointer to the schema registry that corresponds to the signin
  // profile.
  //
  // After this method is called, the component cloud policy manager becomes
  // associated with this schema registry.
  void SetSigninProfileSchemaRegistry(SchemaRegistry* schema_registry);

  // Sets whether the component cloud policy should be disabled (by skipping
  // the component cloud policy service creation).
  void set_component_policy_disabled_for_testing(
      bool component_policy_disabled_for_testing) {
    component_policy_disabled_for_testing_ =
        component_policy_disabled_for_testing;
  }

  // Return a pointer to the machine certificate uploader. The callers do
  // not take ownership of that pointer.
  ash::attestation::MachineCertificateUploader*
  GetMachineCertificateUploader() {
    return machine_certificate_uploader_.get();
  }

  // Called when UserManager is created.
  void OnUserManagerCreated(user_manager::UserManager* user_manager);
  // Called just before UserManager is destroyed.
  void OnUserManagerWillBeDestroyed();

  // user_manager::UserManager::Observer:
  void OnUserToBeRemoved(const AccountId& account_id) override;
  void OnUserRemoved(const AccountId& account_id,
                     user_manager::UserRemovalReason reason) override;

  HeartbeatScheduler* GetHeartbeatSchedulerForTesting() const;

  reporting::OsUpdatesReporter* GetOsUpdatesReporter() const;

  reporting::MetricReportingManager* GetMetricReportingManager();

 protected:
  // Object that monitors managed session related events used by reporting
  // services, protected for testing.
  std::unique_ptr<ManagedSessionService> managed_session_service_;

  // Object that reports login/logout events to the server, protected for
  // testing.
  std::unique_ptr<ash::reporting::LoginLogoutReporter> login_logout_reporter_;

  // Object that reports user added/removed events to the server, protected for
  // testing.
  std::unique_ptr<reporting::UserAddedRemovedReporter>
      user_added_removed_reporter_;

  // Object that reports user lock/unlock events to the server, protected for
  // testing.
  std::unique_ptr<ash::reporting::LockUnlockReporter> lock_unlock_reporter_;

  // Object that handles reporting of ChromeOS updates, protected for
  // testing.
  std::unique_ptr<reporting::OsUpdatesReporter> os_updates_reporter_;

 private:
  // Caches removed users. Passed to the reporter, when it is created.
  struct RemovedUser {
    std::string user_email;  // Maybe empty, if the user should not be reported.
    user_manager::UserRemovalReason reason;
  };

  // Saves the state keys received from |session_manager_client_|.
  void OnStateKeysUpdated();

  void NotifyConnected();
  void NotifyGotRegistry();

  // Factory function to create the StatusUploader.
  void CreateStatusUploader(ManagedSessionService* managed_session_service);

  // Init |managed_session_service_| and reporting objects such as
  // |login_logout_reporter_|, |user_added_removed_reporter_| and
  // |lock_unlock_reporter_|.
  void CreateManagedSessionServiceAndReporters();

  // Points to the object owned by the base CloudPolicyManager, but with actual
  // device policy specific type.
  raw_ptr<DeviceCloudPolicyStoreAsh> device_store_;

  // Manages external data referenced by device policies.
  std::unique_ptr<CloudExternalDataManager> external_data_manager_;

  raw_ptr<ServerBackedStateKeysBroker, DanglingUntriaged> state_keys_broker_;

  raw_ptr<StartCrdSessionJobDelegate, DanglingUntriaged> crd_delegate_;

  // Helper object that handles updating the server with our current device
  // state.
  std::unique_ptr<StatusUploader> status_uploader_;

  // Helper object that handles uploading system logs to the server.
  std::unique_ptr<SystemLogUploader> syslog_uploader_;

  // Helper object that handles sending heartbeats over the GCM channel to
  // the server, to monitor connectivity.
  std::unique_ptr<HeartbeatScheduler> heartbeat_scheduler_;

  // Object that initiates device metrics collection and reporting.
  std::unique_ptr<reporting::MetricReportingManager> metric_reporting_manager_;

  // The TaskRunner used to do device status and log uploads.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // PrefService instance to read the policy refresh rate from.
  raw_ptr<PrefService, DanglingUntriaged> local_state_;

  base::CallbackListSubscription state_keys_update_subscription_;

  std::unique_ptr<ReportingUserTracker> reporting_user_tracker_;

  std::unique_ptr<ash::attestation::EnrollmentCertificateUploader>
      enrollment_certificate_uploader_;
  std::unique_ptr<ash::attestation::EnrollmentIdUploadManager>
      enrollment_id_upload_manager_;
  std::unique_ptr<ash::attestation::MachineCertificateUploader>
      machine_certificate_uploader_;
  std::unique_ptr<EuiccStatusUploader> euicc_status_uploader_;

  // Uploader for remote server unlock related lookup keys.
  std::unique_ptr<LookupKeyUploader> lookup_key_uploader_;

  // Wrapper schema registry that will track the signin profile schema registry
  // once it is passed to this class.
  std::unique_ptr<ForwardingSchemaRegistry>
      signin_profile_forwarding_schema_registry_;

  // Whether the component cloud policy should be disabled (by skipping the
  // component cloud policy service creation).
  bool component_policy_disabled_for_testing_ = false;

  // Caches users being removed.
  base::flat_map<AccountId, bool> users_to_be_removed_;

  std::vector<RemovedUser> removed_users_;

  std::unique_ptr<reporting::UserEventReporterHelper> helper_;

  base::ObserverList<Observer, true>::Unchecked observers_;

  base::ScopedObservation<user_manager::UserManager,
                          DeviceCloudPolicyManagerAsh>
      user_manager_observation_{this};
};

}  // namespace policy

namespace base {

template <>
struct ScopedObservationTraits<policy::DeviceCloudPolicyManagerAsh,
                               policy::DeviceCloudPolicyManagerAsh::Observer> {
  static void AddObserver(
      policy::DeviceCloudPolicyManagerAsh* source,
      policy::DeviceCloudPolicyManagerAsh::Observer* observer) {
    source->AddDeviceCloudPolicyManagerObserver(observer);
  }
  static void RemoveObserver(
      policy::DeviceCloudPolicyManagerAsh* source,
      policy::DeviceCloudPolicyManagerAsh::Observer* observer) {
    source->RemoveDeviceCloudPolicyManagerObserver(observer);
  }
};

}  // namespace base

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_CLOUD_POLICY_MANAGER_ASH_H_

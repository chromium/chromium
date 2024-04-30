// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_NETWORKING_EUICC_STATUS_UPLOADER_H_
#define CHROME_BROWSER_ASH_POLICY_NETWORKING_EUICC_STATUS_UPLOADER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_policy_observer.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "net/base/backoff_entry.h"

class PrefService;
class PrefRegistrySimple;

namespace base {
class OneShotTimer;
}

namespace enterprise_management {
class UploadEuiccInfoRequest;
}

namespace policy {

// Class responsible for uploading the information about the current ESim
// profiles to DMServer.
class EuiccStatusUploader : public ash::NetworkPolicyObserver,
                            public ash::HermesManagerClient::Observer,
                            public ash::HermesEuiccClient::Observer,
                            public ash::ManagedCellularPrefHandler::Observer,
                            public CloudPolicyClient::Observer {
 public:
  EuiccStatusUploader(CloudPolicyClient* client, PrefService* local_state);
  ~EuiccStatusUploader() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

 private:
  friend class EuiccStatusUploaderTest;

  // Callback used in tests to mock out check for device provisioning state.
  using IsDeviceActiveCallback = base::RepeatingCallback<bool()>;

  EuiccStatusUploader(CloudPolicyClient* client,
                      PrefService* local_state,
                      IsDeviceActiveCallback is_device_managed_callback);

  // A local state preference that stores the last uploaded Euicc status in the
  // following format:
  // {
  //    euicc_count: integer
  //    esim_profiles: [
  //      iccid : string,
  //      network_name : string,
  //      smdp_activation_code : string,
  //      smds_activation_code : string,
  //    ]
  // }
  // Please note that the |smdp_activation_code| and |smds_activation_code|
  // fields are mutually exclusive.
  static const char kLastUploadedEuiccStatusPref[];

  // A local state boolean preference which determines whether we should set
  // UploadEuiccInfoRequest.clear_profile_list to true. This is set to true when
  // clear EUICC remote command was run on the client.
  static const char kShouldSendClearProfilesRequestPref[];

  // Constructs the proto for the EUICC status request.
  static std::unique_ptr<enterprise_management::UploadEuiccInfoRequest>
  ConstructRequestFromStatus(const base::Value::Dict& status,
                             bool clear_profile_list);

  // ash::NetworkPolicyObserver:
  void PoliciesApplied(const std::string& userhash) override;
  void OnManagedNetworkConfigurationHandlerShuttingDown() override;

  // CloudPolicyClient::Observer:
  void OnRegistrationStateChanged(CloudPolicyClient* client) override;
  void OnPolicyFetched(CloudPolicyClient* client) override;
  void OnClientError(CloudPolicyClient* client) override {}
  void OnServiceAccountSet(CloudPolicyClient* client,
                           const std::string& account_email) override {}

  // ash::HermesManagerClient:
  void OnAvailableEuiccListChanged() override;

  // ash::HermesEuiccClient:
  void OnEuiccReset(const dbus::ObjectPath& euicc_path) override;

  // ash::ManagedCellularPrefHandler:
  void OnManagedCellularPrefChanged() override;

  base::Value::Dict GetCurrentEuiccStatus() const;
  void MaybeUploadStatus();
  void UploadStatus(base::Value::Dict status);
  void OnStatusUploaded(bool should_send_clear_profiles_request, bool success);
  void RetryUpload();

  // Used in tests. Fires |retry_timer_| to avoid flakiness.
  void FireRetryTimerIfExistsForTesting();

  raw_ptr<CloudPolicyClient> client_;
  raw_ptr<PrefService> local_state_;

  bool currently_uploading_ = false;
  // The status that is being uploaded right now.
  base::Value::Dict attempted_upload_status_;
  bool is_policy_fetched_ = false;
  IsDeviceActiveCallback is_device_managed_callback_;

  // Timer which will try to reupload the status after a while.
  std::unique_ptr<base::OneShotTimer> retry_timer_;
  net::BackoffEntry retry_entry_;

  base::ScopedObservation<ash::HermesManagerClient,
                          ash::HermesManagerClient::Observer>
      hermes_manager_observation_{this};
  base::ScopedObservation<ash::HermesEuiccClient,
                          ash::HermesEuiccClient::Observer>
      hermes_euicc_observation_{this};
  base::ScopedObservation<CloudPolicyClient, CloudPolicyClient::Observer>
      cloud_policy_client_observation_{this};

  raw_ptr<ash::ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_ = nullptr;

  base::WeakPtrFactory<EuiccStatusUploader> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_NETWORKING_EUICC_STATUS_UPLOADER_H_

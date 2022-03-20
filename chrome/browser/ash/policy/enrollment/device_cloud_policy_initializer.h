// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_DEVICE_CLOUD_POLICY_INITIALIZER_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_DEVICE_CLOUD_POLICY_INITIALIZER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class PrefService;

namespace ash {
class InstallAttributes;
}

namespace chromeos {
namespace system {
class StatisticsProvider;
}
}  // namespace chromeos

namespace policy {
class DeviceCloudPolicyStoreAsh;
class DeviceManagementService;
struct EnrollmentConfig;

// The |DeviceCloudPolicyInitializer| is a helper class which calls
// `DeviceCloudPolicyManager::StartConnection` with a new `CloudPolicyClient`
// for a given |DeviceManagementService|. It does so, once
// - the `DeviceCloudPolicyStoreAsh` is initialized and has policy,
// - the `ServerBackedStateKeysBroker` is available,
// - `ash::InstallAttributes::IsActiveDirectoryManaged()` == false.
//
// It is expected that the |DeviceCloudPolicyInitializer| will be
// destroyed soon after it called |StartConnection|, but see
// crbug.com/705758 for complications.
class DeviceCloudPolicyInitializer
    : public CloudPolicyStore::Observer,
      public DeviceCloudPolicyManagerAsh::Observer {
 public:
  DeviceCloudPolicyInitializer(
      PrefService* local_state,
      DeviceManagementService* enterprise_service,
      ash::InstallAttributes* install_attributes,
      ServerBackedStateKeysBroker* state_keys_broker,
      DeviceCloudPolicyStoreAsh* policy_store,
      DeviceCloudPolicyManagerAsh* policy_manager,
      chromeos::system::StatisticsProvider* statistics_provider);

  DeviceCloudPolicyInitializer(const DeviceCloudPolicyInitializer&) = delete;
  DeviceCloudPolicyInitializer& operator=(const DeviceCloudPolicyInitializer&) =
      delete;

  ~DeviceCloudPolicyInitializer() override;

  virtual void Init();
  virtual void Shutdown();

  // Get the enrollment configuration that has been set up via signals such as
  // device requisition, OEM manifest, pre-existing installation-time attributes
  // or server-backed state retrieval. The configuration is stored in |config|,
  // |config.mode| will be MODE_NONE if there is no prescribed configuration.
  // |config.management_domain| will contain the domain the device is supposed
  // to be enrolled to as decided by factors such as forced re-enrollment,
  // enrollment recovery, or already-present install attributes. Note that
  // |config.management_domain| may be non-empty even if |config.mode| is
  // MODE_NONE.
  // TODO(crbug.com/1236174): Remove EnrollmentConfig from initializer.
  EnrollmentConfig GetPrescribedEnrollmentConfig() const;

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

  // DeviceCloudPolicyManagerAsh::Observer
  void OnDeviceCloudPolicyManagerConnected() override;
  void OnDeviceCloudPolicyManagerDisconnected() override;
  void OnDeviceCloudPolicyManagerGotRegistry() override;

  void SetSystemURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory);

 private:
  // Creates a new CloudPolicyClient.
  std::unique_ptr<CloudPolicyClient> CreateClient(
      DeviceManagementService* device_management_service);

  void TryToStartConnection();
  void StartConnection(std::unique_ptr<CloudPolicyClient> client);

  // Get a machine flag from |statistics_provider_|, returning the given
  // |default_value| if not present.
  bool GetMachineFlag(const std::string& key, bool default_value) const;

  PrefService* local_state_;
  DeviceManagementService* enterprise_service_;
  ash::InstallAttributes* install_attributes_;
  ServerBackedStateKeysBroker* state_keys_broker_;
  DeviceCloudPolicyStoreAsh* policy_store_;
  DeviceCloudPolicyManagerAsh* policy_manager_;
  chromeos::system::StatisticsProvider* statistics_provider_;
  bool is_initialized_ = false;
  bool policy_manager_store_ready_notified_ = false;

  base::CallbackListSubscription state_keys_update_subscription_;
  base::ScopedObservation<
      DeviceCloudPolicyManagerAsh,
      DeviceCloudPolicyManagerAsh::Observer,
      &DeviceCloudPolicyManagerAsh::AddDeviceCloudPolicyManagerObserver,
      &DeviceCloudPolicyManagerAsh::RemoveDeviceCloudPolicyManagerObserver>
      policy_manager_observer_{this};

  // The URLLoaderFactory set in tests.
  scoped_refptr<network::SharedURLLoaderFactory>
      system_url_loader_factory_for_testing_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_DEVICE_CLOUD_POLICY_INITIALIZER_H_

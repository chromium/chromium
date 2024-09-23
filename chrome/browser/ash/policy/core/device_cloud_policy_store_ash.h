// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_CLOUD_POLICY_STORE_ASH_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_CLOUD_POLICY_STORE_ASH_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_validator.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

namespace ash {
class InstallAttributes;
}

namespace base {
class SequencedTaskRunner;
}

namespace enterprise_management {
class PolicyFetchResponse;
}

namespace features {
BASE_DECLARE_FEATURE(kDeviceIdValidation);
}  // namespace features

namespace policy {

// CloudPolicyStore implementation for device policy on Ash. Policy is
// stored/loaded via D-Bus to/from session_manager.
// TODO(tnagel): Either drop "Cloud" from the name or refactor.
class DeviceCloudPolicyStoreAsh : public CloudPolicyStore,
                                  public ash::DeviceSettingsService::Observer {
 public:
  DeviceCloudPolicyStoreAsh(
      ash::DeviceSettingsService* device_settings_service,
      ash::InstallAttributes* install_attributes,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  DeviceCloudPolicyStoreAsh(const DeviceCloudPolicyStoreAsh&) = delete;
  DeviceCloudPolicyStoreAsh& operator=(const DeviceCloudPolicyStoreAsh&) =
      delete;

  ~DeviceCloudPolicyStoreAsh() override;

  // CloudPolicyStore:
  // Note that Store() must not be called before the store gets initialized (by
  // means of either Load() or InstallInitialPolicy()).
  void Store(const enterprise_management::PolicyFetchResponse& policy) override;
  void Load() override;

  // Installs initial policy. This is different from Store() in that it skips
  // the signature validation step against already-installed policy. The checks
  // against installation-time attributes are performed nevertheless. The result
  // of the operation is reported through the OnStoreLoaded() or OnStoreError()
  // observer callbacks.
  void InstallInitialPolicy(
      const enterprise_management::PolicyFetchResponse& policy);

  // ash::DeviceSettingsService::Observer:
  void DeviceSettingsUpdated() override;
  void OnDeviceSettingsServiceShutdown() override;

  // CloudPolicyStore:
  void UpdateFirstPoliciesLoaded() override;

 private:
  // Create a validator for |policy| with basic device policy configuration and
  // OnPolicyStored() as the completion callback.
  std::unique_ptr<DeviceCloudPolicyValidator> CreateValidator(
      const enterprise_management::PolicyFetchResponse& policy);

  // Called on completion on the policy validation prior to storing policy.
  // Starts the actual store operation.
  void OnPolicyToStoreValidated(DeviceCloudPolicyValidator* validator);

  // Handles store completion operations updates status.
  void OnPolicyStored();

  // Re-syncs policy and status from |device_settings_service_|.
  void UpdateFromService();

  // Set |status_| based on device_settings_service_->status().
  void UpdateStatusFromService();

  // For enterprise devices, once per session, validate internal consistency of
  // enrollment state (DM token must be present on enrolled devices) and in case
  // of failure set flag to indicate that recovery is required.
  void CheckDMToken();

  // Whether DM token check has yet been done.
  bool dm_token_checked_ = false;

  raw_ptr<ash::DeviceSettingsService> device_settings_service_;
  raw_ptr<ash::InstallAttributes> install_attributes_;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  base::WeakPtrFactory<DeviceCloudPolicyStoreAsh> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_CLOUD_POLICY_STORE_ASH_H_

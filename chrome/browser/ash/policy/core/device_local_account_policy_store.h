// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_POLICY_STORE_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_POLICY_STORE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/user_cloud_policy_store_base.h"

namespace base {
class SequencedTaskRunner;
}

namespace enterprise_management {
class PolicyFetchResponse;
}

namespace policy {

// CloudPolicyStore implementation for device-local account policy. Stores/loads
// policy to/from session_manager.
class DeviceLocalAccountPolicyStore : public UserCloudPolicyStoreBase {
 public:
  DeviceLocalAccountPolicyStore(
      const std::string& account_id,
      ash::SessionManagerClient* client,
      ash::DeviceSettingsService* device_settings_service,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  DeviceLocalAccountPolicyStore(const DeviceLocalAccountPolicyStore&) = delete;
  DeviceLocalAccountPolicyStore& operator=(
      const DeviceLocalAccountPolicyStore&) = delete;

  ~DeviceLocalAccountPolicyStore() override;

  const std::string& account_id() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return account_id_;
  }

  // CloudPolicyStore:
  void Store(const enterprise_management::PolicyFetchResponse& policy) override;
  void Load() override;

  // Loads the policy synchronously on the current thread.
  void LoadImmediately();

 protected:
  // UserCloudPolicyStoreBase:
  std::unique_ptr<UserCloudPolicyValidator> CreateValidator(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy,
      CloudPolicyValidatorBase::ValidateTimestampOption option) override;

 private:
  // The callback invoked once policy validation is complete. Passed are the
  // used public key and the validator.
  using ValidateCompletionCallback =
      base::OnceCallback<void(const std::string&, UserCloudPolicyValidator*)>;

  // Called back by |session_manager_client_| after policy retrieval. Checks for
  // success and triggers policy validation.
  void ValidateLoadedPolicyBlob(
      bool validate_in_background,
      ash::SessionManagerClient::RetrievePolicyResponseType response_type,
      const std::string& policy_blob);

  // Updates state after validation and notifies observers.
  void UpdatePolicy(const std::string& signature_validation_public_key,
                    UserCloudPolicyValidator* validator);

  // Sends the policy blob to session_manager for storing after validation.
  void OnPolicyToStoreValidated(
      const std::string& signature_validation_public_key_unused,
      UserCloudPolicyValidator* validator);

  // Called back when a store operation completes, updates state and reloads the
  // policy if applicable.
  void HandleStoreResult(bool result);

  // Gets the owner key and triggers policy validation.
  void CheckKeyAndValidate(
      bool valid_timestamp_required,
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy,
      bool validate_in_background,
      ValidateCompletionCallback callback);

  // Triggers policy validation.
  void Validate(
      bool valid_timestamp_required,
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy,
      ValidateCompletionCallback callback,
      ash::DeviceSettingsService::OwnershipStatus ownership_status);

  const std::string account_id_;
  raw_ptr<ash::SessionManagerClient> session_manager_client_;
  raw_ptr<ash::DeviceSettingsService> device_settings_service_;

  base::WeakPtrFactory<DeviceLocalAccountPolicyStore> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_POLICY_STORE_H_

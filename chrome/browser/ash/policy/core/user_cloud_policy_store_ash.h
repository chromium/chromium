// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_USER_CLOUD_POLICY_STORE_ASH_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_USER_CLOUD_POLICY_STORE_ASH_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/user_cloud_policy_store_base.h"

namespace ash {
class CryptohomeMiscClient;
}

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class CachedPolicyKeyLoader;

// Implements a policy store backed by the Chrome OS' session_manager, which
// takes care of persisting policy to disk and is accessed via DBus calls
// through SessionManagerClient.
class UserCloudPolicyStoreAsh : public UserCloudPolicyStoreBase {
 public:
  UserCloudPolicyStoreAsh(
      ash::CryptohomeMiscClient* cryptohome_misc_client,
      ash::SessionManagerClient* session_manager_client,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const AccountId& account_id,
      const base::FilePath& user_policy_key_dir);

  UserCloudPolicyStoreAsh(const UserCloudPolicyStoreAsh&) = delete;
  UserCloudPolicyStoreAsh& operator=(const UserCloudPolicyStoreAsh&) = delete;

  ~UserCloudPolicyStoreAsh() override;

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
  // Starts validation of |policy| before storing it.
  void ValidatePolicyForStore(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy);

  // Completion handler for policy validation on the Store() path.
  // Starts a store operation if the validation succeeded.
  void OnPolicyToStoreValidated(UserCloudPolicyValidator* validator);

  // Called back from SessionManagerClient for policy store operations.
  void OnPolicyStored(bool success);

  // Called back from SessionManagerClient for policy load operations.
  void OnPolicyRetrieved(
      ash::SessionManagerClient::RetrievePolicyResponseType response,
      const std::string& policy_blob);

  // Starts validation of the loaded |policy| before installing it.
  void ValidateRetrievedPolicy(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy);

  // Completion handler for policy validation on the Load() path. Installs the
  // policy and publishes it if validation succeeded.
  void OnRetrievedPolicyValidated(UserCloudPolicyValidator* validator);

  std::unique_ptr<UserCloudPolicyValidator> CreateValidatorForLoad(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy);

  raw_ptr<ash::SessionManagerClient> session_manager_client_;
  const AccountId account_id_;

  // Used to load the policy key provided by session manager as a file.
  std::unique_ptr<CachedPolicyKeyLoader> cached_policy_key_loader_;

  base::WeakPtrFactory<UserCloudPolicyStoreAsh> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_USER_CLOUD_POLICY_STORE_ASH_H_

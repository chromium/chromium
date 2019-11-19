// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_PRE_SIGNIN_POLICY_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_PRE_SIGNIN_POLICY_FETCHER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/policy/cached_policy_key_loader_chromeos.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"

namespace base {
class SequencedTaskRunner;
}

namespace chromeos {
class CryptohomeClient;
}

namespace enterprise_management {
class CloudPolicySettings;
class PolicyData;
class PolicyFetchResponse;
}  // namespace enterprise_management

namespace policy {

// Performs a policy fetch before actually mounting cryptohome. This is done in
// the following steps:
// (1) Mount the user's cryptohome to a temporary location.
// (2) Retrieve policy from there, along with the policy verification key.
// (3) Unmount the temporary cryptohome.
//
// And if cached policy was found:
// (4) Validate the cached policy.
// (5) Try to fetch fresh policy from DMServer.
// (6) Validate fresh policy.
//
// If steps (1)-(4) are successful, the cached policy will be used even if no
// fresh policy is available.
class PreSigninPolicyFetcher : public CloudPolicyClient::Observer {
 public:
  // The result of the pre-signin policy fetch.
  enum class PolicyFetchResult {
    // The policy could not be fetched, e.g. error in cryptohome/session_manager
    // calls or it could not be validated.
    ERROR,
    // The user does not have policy.
    NO_POLICY,
    // The policy was successfully fetched. This could be a cached policy or a
    // fresh policy.
    SUCCESS,
  };

  using PolicyFetchResultCallback = base::OnceCallback<void(
      PolicyFetchResult result,
      std::unique_ptr<enterprise_management::CloudPolicySettings> policy_data)>;

  PreSigninPolicyFetcher(chromeos::CryptohomeClient* cryptohome_client,
                         chromeos::SessionManagerClient* session_manager_client,
                         std::unique_ptr<CloudPolicyClient> cloud_policy_client,
                         bool is_active_directory_managed,
                         const AccountId& account_id,
                         const cryptohome::KeyDefinition& auth_key);
  ~PreSigninPolicyFetcher() override;

  // Start the policy fetch procedure. |callback| will be invoked with the
  // result.
  void FetchPolicy(PolicyFetchResultCallback callback);

  // Forces a policy fetch timeout.
  // Returns true if the timer was running and a timeout was forced. If the
  // timer was not running, returns false.
  bool ForceTimeoutForTesting();

 private:
  using RetrievePolicyResponseType =
      chromeos::SessionManagerClient::RetrievePolicyResponseType;

  void OnMountTemporaryUserHome(base::Optional<cryptohome::BaseReply> reply);

  void OnCachedPolicyRetrieved(
      RetrievePolicyResponseType retrieve_policy_response,
      const std::string& policy_blob);

  void OnPolicyKeyLoaded(RetrievePolicyResponseType retrieve_policy_response,
                         const std::string& policy_blob);

  void OnUnmountTemporaryUserHome(
      RetrievePolicyResponseType retrieve_policy_response,
      const std::string& policy_blob,
      base::Optional<cryptohome::BaseReply> reply);

  void OnCachedPolicyValidated(UserCloudPolicyValidator* validator);

  // CloudPolicyClient::Observer:
  void OnPolicyFetched(CloudPolicyClient* client) override;
  void OnRegistrationStateChanged(CloudPolicyClient* client) override;
  void OnClientError(CloudPolicyClient* client) override;

  // Called by |policy_fetch_timeout_|.
  void OnPolicyFetchTimeout();

  void OnFetchedPolicyValidated(UserCloudPolicyValidator* validator);

  // Invokes |callback_| with the passed result after cleaning up.
  void NotifyCallback(
      PolicyFetchResult result,
      std::unique_ptr<enterprise_management::CloudPolicySettings>
          policy_payload);

  // Creates a validator which can be used to validate the cached policy.
  std::unique_ptr<UserCloudPolicyValidator> CreateValidatorForCachedPolicy(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy);
  // Creates a validator which can be used to validate the freshly fetched
  // policy, based on the cached policy.
  std::unique_ptr<UserCloudPolicyValidator> CreateValidatorForFetchedPolicy(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy);

  chromeos::CryptohomeClient* const cryptohome_client_;
  chromeos::SessionManagerClient* const session_manager_client_;
  const std::unique_ptr<CloudPolicyClient> cloud_policy_client_;
  const bool is_active_directory_managed_;
  const AccountId account_id_;
  const cryptohome::KeyDefinition auth_key_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Callback passed to FetchPolicy method.
  PolicyFetchResultCallback callback_;

  // |policy_data_| and |policy_payload_| will first hold the cached policy on
  // successful load and validation, and will then be reset to hold fresh policy
  // if download and validation of the fresh policy is successful.
  std::unique_ptr<enterprise_management::PolicyData> policy_data_;
  std::unique_ptr<enterprise_management::CloudPolicySettings> policy_payload_;

  // A timer that puts a hard limit on the maximum time to wait for the fresh
  // policy fetch.
  base::OneShotTimer policy_fetch_timeout_;

  // Used to load the policy key provided by session manager as a file.
  std::unique_ptr<CachedPolicyKeyLoaderChromeOS> cached_policy_key_loader_;

  base::WeakPtrFactory<PreSigninPolicyFetcher> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PreSigninPolicyFetcher);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_PRE_SIGNIN_POLICY_FETCHER_H_

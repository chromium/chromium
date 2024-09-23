// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/user_cloud_policy_store_ash.h"

#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/policy/core/cached_policy_key_loader.h"
#include "chrome/browser/ash/policy/value_validation/onc_user_policy_value_validator.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/cryptohome/cryptohome_util.h"
#include "chromeos/ash/components/dbus/session_manager/policy_descriptor.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "google_apis/gaia/gaia_auth_util.h"

using RetrievePolicyResponseType =
    ash::SessionManagerClient::RetrievePolicyResponseType;

namespace em = enterprise_management;

namespace policy {

namespace {

// Extracts the domain name from the passed username.
std::string ExtractDomain(const std::string& username) {
  return gaia::ExtractDomainName(gaia::CanonicalizeEmail(username));
}

}  // namespace

UserCloudPolicyStoreAsh::UserCloudPolicyStoreAsh(
    ash::CryptohomeMiscClient* cryptohome_misc_client,
    ash::SessionManagerClient* session_manager_client,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const AccountId& account_id,
    const base::FilePath& user_policy_key_dir)
    : UserCloudPolicyStoreBase(background_task_runner,
                               PolicyScope::POLICY_SCOPE_USER),
      session_manager_client_(session_manager_client),
      account_id_(account_id),
      cached_policy_key_loader_(
          std::make_unique<CachedPolicyKeyLoader>(cryptohome_misc_client,
                                                  background_task_runner,
                                                  account_id,
                                                  user_policy_key_dir)) {}

UserCloudPolicyStoreAsh::~UserCloudPolicyStoreAsh() {}

void UserCloudPolicyStoreAsh::Store(const em::PolicyFetchResponse& policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Cancel all pending requests.
  weak_factory_.InvalidateWeakPtrs();

  std::unique_ptr<em::PolicyFetchResponse> response(
      new em::PolicyFetchResponse(policy));
  cached_policy_key_loader_->EnsurePolicyKeyLoaded(
      base::BindOnce(&UserCloudPolicyStoreAsh::ValidatePolicyForStore,
                     weak_factory_.GetWeakPtr(), std::move(response)));
}

void UserCloudPolicyStoreAsh::Load() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Cancel all pending requests.
  weak_factory_.InvalidateWeakPtrs();

  login_manager::PolicyDescriptor descriptor =
      ash::MakeChromePolicyDescriptor(login_manager::ACCOUNT_TYPE_USER,
                                      cryptohome::GetCryptohomeId(account_id_));
  session_manager_client_->RetrievePolicy(
      descriptor, base::BindOnce(&UserCloudPolicyStoreAsh::OnPolicyRetrieved,
                                 weak_factory_.GetWeakPtr()));
}

std::unique_ptr<UserCloudPolicyValidator>
UserCloudPolicyStoreAsh::CreateValidator(
    std::unique_ptr<em::PolicyFetchResponse> policy,
    CloudPolicyValidatorBase::ValidateTimestampOption option) {
  auto validator =
      UserCloudPolicyStoreBase::CreateValidator(std::move(policy), option);
  validator->ValidateValues(std::make_unique<ONCUserPolicyValueValidator>());
  return validator;
}

void UserCloudPolicyStoreAsh::LoadImmediately() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This blocking D-Bus call is in the startup path and will block the UI
  // thread. This only happens when the Profile is created synchronously, which
  // on Chrome OS happens whenever the browser is restarted into the same
  // session. That happens when the browser crashes, or right after signin if
  // the user has flags configured in about:flags.
  // However, on those paths we must load policy synchronously so that the
  // Profile initialization never sees unmanaged prefs, which would lead to
  // data loss. http://crbug.com/263061
  std::string policy_blob;
  login_manager::PolicyDescriptor descriptor =
      ash::MakeChromePolicyDescriptor(login_manager::ACCOUNT_TYPE_USER,
                                      cryptohome::GetCryptohomeId(account_id_));
  RetrievePolicyResponseType response_type =
      session_manager_client_->BlockingRetrievePolicy(descriptor, &policy_blob);

  if (response_type == RetrievePolicyResponseType::GET_SERVICE_FAIL) {
    LOG(ERROR)
        << "Session manager claims that session doesn't exist; signing out";
    base::debug::DumpWithoutCrashing();
    chrome::AttemptUserExit();
    return;
  }

  if (policy_blob.empty()) {
    // The session manager doesn't have policy, or the call failed.
    NotifyStoreLoaded();
    return;
  }

  std::unique_ptr<em::PolicyFetchResponse> policy(
      new em::PolicyFetchResponse());
  if (!policy->ParseFromString(policy_blob)) {
    status_ = STATUS_PARSE_ERROR;
    NotifyStoreError();
    return;
  }

  if (!cached_policy_key_loader_->LoadPolicyKeyImmediately()) {
    status_ = STATUS_LOAD_ERROR;
    NotifyStoreError();
    return;
  }

  std::unique_ptr<UserCloudPolicyValidator> validator =
      CreateValidatorForLoad(std::move(policy));
  validator->RunValidation();
  OnRetrievedPolicyValidated(validator.get());
}

void UserCloudPolicyStoreAsh::ValidatePolicyForStore(
    std::unique_ptr<em::PolicyFetchResponse> policy) {
  // Create and configure a validator.
  std::unique_ptr<UserCloudPolicyValidator> validator = CreateValidator(
      std::move(policy), CloudPolicyValidatorBase::TIMESTAMP_VALIDATED);
  validator->ValidateUser(account_id_);
  const std::string& cached_policy_key =
      cached_policy_key_loader_->cached_policy_key();
  if (cached_policy_key.empty()) {
    validator->ValidateInitialKey(ExtractDomain(account_id_.GetUserEmail()));
  } else {
    validator->ValidateSignatureAllowingRotation(
        cached_policy_key, ExtractDomain(account_id_.GetUserEmail()));
  }

  // Start validation.
  UserCloudPolicyValidator::StartValidation(
      std::move(validator),
      base::BindOnce(&UserCloudPolicyStoreAsh::OnPolicyToStoreValidated,
                     weak_factory_.GetWeakPtr()));
}

void UserCloudPolicyStoreAsh::OnPolicyToStoreValidated(
    UserCloudPolicyValidator* validator) {
  validation_result_ = validator->GetValidationResult();
  if (!validator->success()) {
    status_ = STATUS_VALIDATION_ERROR;
    NotifyStoreError();
    return;
  }

  std::string policy_blob;
  if (!validator->policy()->SerializeToString(&policy_blob)) {
    status_ = STATUS_SERIALIZE_ERROR;
    NotifyStoreError();
    return;
  }

  session_manager_client_->StorePolicyForUser(
      cryptohome::CreateAccountIdentifierFromAccountId(account_id_),
      policy_blob,
      base::BindOnce(&UserCloudPolicyStoreAsh::OnPolicyStored,
                     weak_factory_.GetWeakPtr()));
}

void UserCloudPolicyStoreAsh::OnPolicyStored(bool success) {
  if (!success) {
    status_ = STATUS_STORE_ERROR;
    NotifyStoreError();
  } else {
    // Load the policy right after storing it, to make sure it was accepted by
    // the session manager. An additional validation is performed after the
    // load; reload the key for that validation too, in case it was rotated.
    cached_policy_key_loader_->ReloadPolicyKey(base::BindOnce(
        &UserCloudPolicyStoreAsh::Load, weak_factory_.GetWeakPtr()));
  }
}

void UserCloudPolicyStoreAsh::OnPolicyRetrieved(
    RetrievePolicyResponseType response_type,
    const std::string& policy_blob) {
  // Disallow the sign in when the Chrome OS user session has not started, which
  // should always happen before the profile construction. An attempt to read
  // the policy outside the session will always fail and return an empty policy
  // blob.
  if (response_type == RetrievePolicyResponseType::GET_SERVICE_FAIL) {
    LOG(ERROR)
        << "Session manager claims that session doesn't exist; signing out";
    base::debug::DumpWithoutCrashing();
    chrome::AttemptUserExit();
    return;
  }

  if (policy_blob.empty()) {
    // session_manager doesn't have policy. Adjust internal state and notify
    // the world about the policy update.
    ResetPolicy();
    policy_map_.Clear();
    policy_signature_public_key_.clear();
    NotifyStoreLoaded();
    return;
  }

  std::unique_ptr<em::PolicyFetchResponse> policy(
      new em::PolicyFetchResponse());
  if (!policy->ParseFromString(policy_blob)) {
    status_ = STATUS_PARSE_ERROR;
    NotifyStoreError();
    return;
  }

  // Load |cached_policy_key_| to verify the loaded policy.
  cached_policy_key_loader_->EnsurePolicyKeyLoaded(
      base::BindOnce(&UserCloudPolicyStoreAsh::ValidateRetrievedPolicy,
                     weak_factory_.GetWeakPtr(), std::move(policy)));
}

void UserCloudPolicyStoreAsh::ValidateRetrievedPolicy(
    std::unique_ptr<em::PolicyFetchResponse> policy) {
  UserCloudPolicyValidator::StartValidation(
      CreateValidatorForLoad(std::move(policy)),
      base::BindOnce(&UserCloudPolicyStoreAsh::OnRetrievedPolicyValidated,
                     weak_factory_.GetWeakPtr()));
}

void UserCloudPolicyStoreAsh::OnRetrievedPolicyValidated(
    UserCloudPolicyValidator* validator) {
  validation_result_ = validator->GetValidationResult();
  if (!validator->success()) {
    status_ = STATUS_VALIDATION_ERROR;
    NotifyStoreError();
    return;
  }

  InstallPolicy(std::move(validator->policy()),
                std::move(validator->policy_data()),
                std::move(validator->payload()),
                cached_policy_key_loader_->cached_policy_key());
  status_ = STATUS_OK;

  NotifyStoreLoaded();
}

std::unique_ptr<UserCloudPolicyValidator>
UserCloudPolicyStoreAsh::CreateValidatorForLoad(
    std::unique_ptr<em::PolicyFetchResponse> policy) {
  std::unique_ptr<UserCloudPolicyValidator> validator = CreateValidator(
      std::move(policy), CloudPolicyValidatorBase::TIMESTAMP_VALIDATED);
  validator->ValidateUser(account_id_);
  // The policy loaded from session manager need not be validated using the
  // verification key since it is secure, and since there may be legacy policy
  // data that was stored without a verification key.
  validator->ValidateSignature(cached_policy_key_loader_->cached_policy_key());
  return validator;
}

}  // namespace policy

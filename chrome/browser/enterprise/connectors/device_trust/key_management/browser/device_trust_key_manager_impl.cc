// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/device_trust_key_manager_impl.h"

#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_rotation_launcher.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_util.h"
#include "crypto/unexportable_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

namespace {

// Wrap the SignSlowly call into this anonymous synchronous function to ensure
// that `str` lives throughout the execution. If that was not being done, `str`
// would get destroyed in the calling sequence and, with span being just a
// pointer, the called sequence would use its data pointer after the address
// was freed up (use-after-free), which is a security issue.
absl::optional<std::vector<uint8_t>> SignString(
    const std::string& str,
    crypto::UnexportableSigningKey* key) {
  if (!key) {
    return absl::nullopt;
  }
  return key->SignSlowly(base::as_bytes(base::make_span(str)));
}

}  // namespace

DeviceTrustKeyManagerImpl::DeviceTrustKeyManagerImpl(
    std::unique_ptr<KeyRotationLauncher> key_rotation_launcher)
    : key_rotation_launcher_(std::move(key_rotation_launcher)),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  DCHECK(key_rotation_launcher_);
}

DeviceTrustKeyManagerImpl::~DeviceTrustKeyManagerImpl() = default;

void DeviceTrustKeyManagerImpl::StartInitialization() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Initialization is only needed when the manager is in its default state
  // with no loaded key.
  if (state_ == InitializationState::kDefault && !key_pair_) {
    // Create a key upon loading failure iff a key was not already successfully
    // created/rotated to prevent trying to re-create a key after failing to
    // load it.
    LoadKey(/*create_on_fail=*/!key_rotation_succeeded_);
  }
}

void DeviceTrustKeyManagerImpl::RotateKey(const std::string& nonce,
                                          RotateKeyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == InitializationState::kDefault) {
    // Update the state right now to mark new client requests as pending.
    state_ = InitializationState::kRotatingKey;

    // Make sure to "drain" the background sequence from pending tasks before
    // attempting to rotate the key. This is done by changing the state (no new
    // incoming requests will be added to the background sequence), and then
    // putting a DoNothing on the sequence and getting the task runner to reply
    // with our callback on the UI Thread after that. This way, once the
    // callback runs, it is running on the UI thread and also the background
    // sequence is empty.
    background_task_runner_->PostTaskAndReply(
        FROM_HERE, base::DoNothing(),
        base::BindOnce(&DeviceTrustKeyManagerImpl::StartKeyRotationInner,
                       weak_factory_.GetWeakPtr(), nonce, std::move(callback)));
    return;
  }

  // Cancel previously pending requests and replace them with this new one.
  if (pending_rotation_request_) {
    std::move(pending_rotation_request_->callback)
        .Run(DeviceTrustKeyManager::KeyRotationResult::CANCELLATION);
  }
  pending_rotation_request_ =
      std::make_unique<DeviceTrustKeyManagerImpl::RotateKeyRequest>(
          nonce, std::move(callback));
}

void DeviceTrustKeyManagerImpl::ExportPublicKeyAsync(
    ExportPublicKeyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsFullyInitialized()) {
    auto public_key_info = key_pair_->key()->GetSubjectPublicKeyInfo();
    std::string public_key(public_key_info.begin(), public_key_info.end());
    std::move(callback).Run(public_key);
    return;
  }

  AddPendingRequest(
      base::BindOnce(&DeviceTrustKeyManagerImpl::ResumeExportPublicKey,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void DeviceTrustKeyManagerImpl::SignStringAsync(const std::string& str,
                                                SignStringCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsFullyInitialized()) {
    background_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&SignString, str, key_pair_->key()),
        std::move(callback));
    return;
  }

  AddPendingRequest(base::BindOnce(&DeviceTrustKeyManagerImpl::ResumeSignString,
                                   weak_factory_.GetWeakPtr(), str,
                                   std::move(callback)));
}

absl::optional<DeviceTrustKeyManagerImpl::KeyMetadata>
DeviceTrustKeyManagerImpl::GetLoadedKeyMetadata() const {
  if (!IsFullyInitialized()) {
    return absl::nullopt;
  }

  const auto& spki_bytes = key_pair_->key()->GetSubjectPublicKeyInfo();
  return DeviceTrustKeyManagerImpl::KeyMetadata{
      key_pair_->trust_level(), key_pair_->key()->Algorithm(),
      std::string(spki_bytes.begin(), spki_bytes.end()),
      sync_key_response_code_};
}

void DeviceTrustKeyManagerImpl::AddPendingRequest(
    base::OnceClosure pending_request) {
  if (pending_request.is_null()) {
    return;
  }

  // Unsafe is fine as long as the pending closures are bound to a weak pointer.
  pending_client_requests_.AddUnsafe(std::move(pending_request));

  // Hook to allow the manager to fix itself if it is in a bad state.
  StartInitialization();
}

void DeviceTrustKeyManagerImpl::LoadKey(bool create_on_fail) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = InitializationState::kLoadingKey;
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&LoadPersistedKey),
      base::BindOnce(&DeviceTrustKeyManagerImpl::OnKeyLoaded,
                     weak_factory_.GetWeakPtr(), create_on_fail));
}

void DeviceTrustKeyManagerImpl::OnKeyLoaded(
    bool create_on_fail,
    std::unique_ptr<SigningKeyPair> loaded_key_pair) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (loaded_key_pair && !loaded_key_pair->is_empty()) {
    key_pair_ = std::move(loaded_key_pair);

    // Kick off key synchronization in the background as non-blocking.
    key_rotation_launcher_->SynchronizePublicKey(
        *key_pair_,
        base::BindOnce(&DeviceTrustKeyManagerImpl::OnSynchronizationFinished,
                       weak_factory_.GetWeakPtr()));
  } else {
    key_pair_.reset();
    sync_key_response_code_ = absl::nullopt;
  }

  state_ = InitializationState::kDefault;
  LogKeyLoadingResult(GetLoadedKeyMetadata());

  // Do this check after caching the previous key as failure to rotate will
  // restore it in persistence.
  if (TryResumePendingRotationRequest()) {
    // In this edge case, a rotate key request came in at the same time
    // as the key was being loaded. In this case, just start a rotate flow again
    // using the pending nonce. The function can be called directly as the
    // background sequence is guaranteed to be empty at this point.
    return;
  }

  if (!IsFullyInitialized() && create_on_fail) {
    // Key loading failed, so we can kick-off the key creation. This is
    // guarded by a flag to make sure not to loop infinitely over:
    // create succeeds -> load fails -> create again...
    StartKeyRotationInner(/*nonce=*/std::string(),
                          /*callback=*/base::DoNothing());
    return;
  }

  // Respond to callbacks. If a key was loaded, these callbacks will be
  // successfully answered to. If a key was not loaded, then might as well
  // respond with a failure instead of keeping them waiting even longer.
  ResumePendingCallbacks();
}

void DeviceTrustKeyManagerImpl::OnSynchronizationFinished(
    absl::optional<int> response_code) {
  sync_key_response_code_ = response_code;
}

void DeviceTrustKeyManagerImpl::StartKeyRotationInner(
    const std::string& nonce,
    RotateKeyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = InitializationState::kRotatingKey;
  key_rotation_succeeded_ = false;

  KeyRotationCommand::Callback rotation_finished_callback =
      base::BindOnce(&DeviceTrustKeyManagerImpl::OnKeyRotationFinished,
                     weak_factory_.GetWeakPtr(), nonce, std::move(callback));

  key_rotation_launcher_->LaunchKeyRotation(
      nonce, std::move(rotation_finished_callback));
}

void DeviceTrustKeyManagerImpl::OnKeyRotationFinished(
    const std::string& nonce,
    RotateKeyCallback callback,
    KeyRotationCommand::Status result_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LogKeyRotationResult(/*had_nonce=*/!nonce.empty(), result_status);

  key_rotation_succeeded_ =
      result_status == KeyRotationCommand::Status::SUCCEEDED;
  state_ = InitializationState::kDefault;

  std::move(callback).Run(
      key_rotation_succeeded_
          ? DeviceTrustKeyManager::KeyRotationResult::SUCCESS
          : DeviceTrustKeyManager::KeyRotationResult::FAILURE);

  if (!key_rotation_succeeded_ && TryResumePendingRotationRequest()) {
    // In this edge case, another rotate key request came in at the same time
    // as the current request was being executed. In this case, just go
    // through the rotate flow again using the new nonce. The rotate flow can be
    // started directly as the background sequence is guaranteed to be empty at
    // this point.
    // This specific case is guarded behind the current rotation process
    // having failed because, if it had succeeded, the LoadKey below would get
    // invoked and would eventually start the rotation as well.
    return;
  }

  if (key_rotation_succeeded_) {
    // In normal key creation/rotation flow, loading the key after having
    // updated persistence is a pretty straightforward thing to do.
    // In the event where `pending_rotation_nonce_` still has a value (meaning
    // there was a concurrent rotate request), LoadKey will take care of that
    // pending request in its own flow. This is better than kicking-off rotation
    // directly from here in the case where the pending rotation request fails -
    // then the manager would have already loaded the key that will remain
    // persisted.
    LoadKey(/*create_on_fail=*/false);
    return;
  }

  // Two possible failure scenarios:
  // - Either there was no key before and key creation failed,
  // - Or there was a previous key and creation of a new key failed, so the old
  //   key was restored.
  // In both cases, the manager doesn't need to try and reload the key (its
  // loaded key is already either null or the old key).
  // Just respond to the pending callbacks - if a key is still set, it will
  // successfully respond to them.
  ResumePendingCallbacks();
}

bool DeviceTrustKeyManagerImpl::TryResumePendingRotationRequest() {
  if (pending_rotation_request_) {
    StartKeyRotationInner(pending_rotation_request_->nonce,
                          std::move(pending_rotation_request_->callback));
    pending_rotation_request_.reset();
    return true;
  }
  return false;
}

void DeviceTrustKeyManagerImpl::ResumePendingCallbacks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_client_requests_.Notify();
}

void DeviceTrustKeyManagerImpl::ResumeExportPublicKey(
    ExportPublicKeyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsFullyInitialized()) {
    ExportPublicKeyAsync(std::move(callback));
  } else {
    std::move(callback).Run(absl::nullopt);
  }
}

void DeviceTrustKeyManagerImpl::ResumeSignString(const std::string& str,
                                                 SignStringCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsFullyInitialized()) {
    SignStringAsync(str, std::move(callback));
  } else {
    std::move(callback).Run(absl::nullopt);
  }
}

bool DeviceTrustKeyManagerImpl::IsFullyInitialized() const {
  return state_ == InitializationState::kDefault && key_pair_ &&
         key_pair_->key();
}

DeviceTrustKeyManagerImpl::RotateKeyRequest::RotateKeyRequest(
    const std::string& nonce_param,
    RotateKeyCallback callback_param)
    : nonce(nonce_param), callback(std::move(callback_param)) {}

DeviceTrustKeyManagerImpl::RotateKeyRequest::~RotateKeyRequest() = default;

}  // namespace enterprise_connectors

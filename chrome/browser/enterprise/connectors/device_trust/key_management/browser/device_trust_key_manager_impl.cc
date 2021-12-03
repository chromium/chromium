// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/device_trust_key_manager_impl.h"

#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_rotation_launcher.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
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

void DeviceTrustKeyManagerImpl::StartKeyRotation(const std::string& nonce) {
  NOTIMPLEMENTED();
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

  return DeviceTrustKeyManagerImpl::KeyMetadata{key_pair_->trust_level(),
                                                key_pair_->key()->Algorithm()};
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
      FROM_HERE, base::BindOnce(&SigningKeyPair::LoadPersistedKey),
      base::BindOnce(&DeviceTrustKeyManagerImpl::OnKeyLoaded,
                     weak_factory_.GetWeakPtr(), create_on_fail));
}

void DeviceTrustKeyManagerImpl::OnKeyLoaded(
    bool create_on_fail,
    std::unique_ptr<SigningKeyPair> loaded_key_pair) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (loaded_key_pair && !loaded_key_pair->is_empty()) {
    key_pair_ = std::move(loaded_key_pair);
  } else {
    key_pair_.reset();
  }

  state_ = InitializationState::kDefault;

  if (!IsFullyInitialized() && create_on_fail) {
    // Key loading failed, so we can kick-off the key creation. This is
    // guarded by a flag to make sure not to loop infinitely over:
    // create succeeds -> load fails -> create again...
    StartKeyRotationInner(/*nonce=*/std::string());
    return;
  }

  LogKeyLoadingResult(GetLoadedKeyMetadata());

  // Respond to callbacks. If a key was loaded, these callbacks will be
  // successfully answered to. If a key was not loaded, then might as well
  // respond with a failure instead of keeping them waiting even longer.
  ResumePendingCallbacks();
}

void DeviceTrustKeyManagerImpl::StartKeyRotationInner(
    const std::string& nonce) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = InitializationState::kRotatingKey;
  key_rotation_succeeded_ = false;

  KeyRotationCommand::Callback rotation_finished_callback =
      base::BindOnce(&DeviceTrustKeyManagerImpl::OnKeyRotationFinished,
                     weak_factory_.GetWeakPtr(), !nonce.empty());

  key_rotation_launcher_->LaunchKeyRotation(
      nonce, std::move(rotation_finished_callback));
}

void DeviceTrustKeyManagerImpl::OnKeyRotationFinished(
    bool had_nonce,
    KeyRotationCommand::Status result_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  key_rotation_succeeded_ =
      result_status == KeyRotationCommand::Status::SUCCEEDED;
  state_ = InitializationState::kDefault;

  LogKeyRotationResult(had_nonce, result_status);

  if (key_rotation_succeeded_) {
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

}  // namespace enterprise_connectors

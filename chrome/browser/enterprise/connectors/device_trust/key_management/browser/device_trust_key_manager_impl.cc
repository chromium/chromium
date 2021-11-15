// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/device_trust_key_manager_impl.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_rotation_launcher.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "crypto/unexportable_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

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
    LoadKey();
  }
}

void DeviceTrustKeyManagerImpl::StartKeyRotation(const std::string& nonce) {
  NOTIMPLEMENTED();
}

void DeviceTrustKeyManagerImpl::ExportPublicKeyAsync(
    ExportPublicKeyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_fully_initialized()) {
    auto public_key_info = key_pair_->key()->GetSubjectPublicKeyInfo();
    std::string public_key(public_key_info.begin(), public_key_info.end());
    std::move(callback).Run(public_key);
    return;
  }

  // TODO(b/204914180): Handle the requests based on the current state (queue,
  // up requests, or retry loading the key).
  std::move(callback).Run(absl::nullopt);
}

void DeviceTrustKeyManagerImpl::SignStringAsync(const std::string& str,
                                                SignStringCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_fully_initialized()) {
    background_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&crypto::UnexportableSigningKey::SignSlowly,
                       base::Unretained(key_pair_->key()),
                       base::as_bytes(base::make_span(str))),
        std::move(callback));
    return;
  }

  // TODO(b/204914180): Handle the requests based on the current state (queue,
  // up requests, or retry loading the key).
  std::move(callback).Run(absl::nullopt);
}

void DeviceTrustKeyManagerImpl::LoadKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = InitializationState::kLoadingKey;
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&SigningKeyPair::LoadPersistedKey),
      base::BindOnce(&DeviceTrustKeyManagerImpl::OnKeyLoaded,
                     weak_factory_.GetWeakPtr()));
}

void DeviceTrustKeyManagerImpl::OnKeyLoaded(
    std::unique_ptr<SigningKeyPair> loaded_key_pair) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (loaded_key_pair) {
    key_pair_ = std::move(loaded_key_pair);
    state_ = InitializationState::kDefault;
    return;
  }

  // Key loading failed, so we can kick-off the key creation.
  StartKeyRotationInner(std::string());
}

void DeviceTrustKeyManagerImpl::StartKeyRotationInner(
    const std::string& nonce) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/204914180): Update logic once key creation can provide status
  // updates.
  state_ = InitializationState::kStartingKeyRotation;
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&KeyRotationLauncher::LaunchKeyRotation,
                     base::Unretained(key_rotation_launcher_.get()), nonce),
      base::BindOnce(&DeviceTrustKeyManagerImpl::OnKeyRotationStarted,
                     weak_factory_.GetWeakPtr()));
}

void DeviceTrustKeyManagerImpl::OnKeyRotationStarted(bool rotation_started) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = InitializationState::kWaitingForKeyRotation;
}

}  // namespace enterprise_connectors

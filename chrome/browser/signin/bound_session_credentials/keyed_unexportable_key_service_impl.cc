// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/keyed_unexportable_key_service_impl.h"

#include <memory>

#include "base/functional/callback.h"

KeyedUnexportableKeyServiceImpl::KeyedUnexportableKeyServiceImpl(
    unexportable_keys::UnexportableKeyTaskManager& task_manager)
    : key_service_implementation_(task_manager) {}

KeyedUnexportableKeyServiceImpl::~KeyedUnexportableKeyServiceImpl() = default;

void KeyedUnexportableKeyServiceImpl::GenerateSigningKeySlowlyAsync(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    unexportable_keys::BackgroundTaskPriority priority,
    base::OnceCallback<void(unexportable_keys::ServiceErrorOr<
                            unexportable_keys::UnexportableKeyId>)> callback) {
  key_service_implementation_.GenerateSigningKeySlowlyAsync(
      acceptable_algorithms, priority, std::move(callback));
}

void KeyedUnexportableKeyServiceImpl::FromWrappedSigningKeySlowlyAsync(
    base::span<const uint8_t> wrapped_key,
    unexportable_keys::BackgroundTaskPriority priority,
    base::OnceCallback<void(unexportable_keys::ServiceErrorOr<
                            unexportable_keys::UnexportableKeyId>)> callback) {
  key_service_implementation_.FromWrappedSigningKeySlowlyAsync(
      wrapped_key, priority, std::move(callback));
}

void KeyedUnexportableKeyServiceImpl::SignSlowlyAsync(
    const unexportable_keys::UnexportableKeyId& key_id,
    base::span<const uint8_t> data,
    unexportable_keys::BackgroundTaskPriority priority,
    base::OnceCallback<void(
        unexportable_keys::ServiceErrorOr<std::vector<uint8_t>>)> callback) {
  key_service_implementation_.SignSlowlyAsync(key_id, data, priority,
                                              std::move(callback));
}

unexportable_keys::ServiceErrorOr<std::vector<uint8_t>>
KeyedUnexportableKeyServiceImpl::GetSubjectPublicKeyInfo(
    unexportable_keys::UnexportableKeyId key_id) const {
  return key_service_implementation_.GetSubjectPublicKeyInfo(key_id);
}

unexportable_keys::ServiceErrorOr<std::vector<uint8_t>>
KeyedUnexportableKeyServiceImpl::GetWrappedKey(
    unexportable_keys::UnexportableKeyId key_id) const {
  return key_service_implementation_.GetWrappedKey(key_id);
}

unexportable_keys::ServiceErrorOr<crypto::SignatureVerifier::SignatureAlgorithm>
KeyedUnexportableKeyServiceImpl::GetAlgorithm(
    unexportable_keys::UnexportableKeyId key_id) const {
  return key_service_implementation_.GetAlgorithm(key_id);
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/fake_keyed_unexportable_key_service.h"

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/signature_verifier.h"

void FakeKeyedUnexportableKeyService::GenerateSigningKeySlowlyAsync(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    unexportable_keys::BackgroundTaskPriority priority,
    base::OnceCallback<void(unexportable_keys::ServiceErrorOr<
                            unexportable_keys::UnexportableKeyId>)> callback) {
  std::move(callback).Run(
      base::unexpected(unexportable_keys::ServiceError::kKeyNotFound));
}

void FakeKeyedUnexportableKeyService::FromWrappedSigningKeySlowlyAsync(
    base::span<const uint8_t> wrapped_key,
    unexportable_keys::BackgroundTaskPriority priority,
    base::OnceCallback<void(unexportable_keys::ServiceErrorOr<
                            unexportable_keys::UnexportableKeyId>)> callback) {
  std::move(callback).Run(
      base::unexpected(unexportable_keys::ServiceError::kKeyNotFound));
}

void FakeKeyedUnexportableKeyService::SignSlowlyAsync(
    const unexportable_keys::UnexportableKeyId& key_id,
    base::span<const uint8_t> data,
    unexportable_keys::BackgroundTaskPriority priority,
    base::OnceCallback<void(
        unexportable_keys::ServiceErrorOr<std::vector<uint8_t>>)> callback) {
  std::move(callback).Run(
      base::unexpected(unexportable_keys::ServiceError::kKeyNotFound));
}

unexportable_keys::ServiceErrorOr<std::vector<uint8_t>>
FakeKeyedUnexportableKeyService::GetSubjectPublicKeyInfo(
    unexportable_keys::UnexportableKeyId key_id) const {
  return base::unexpected(unexportable_keys::ServiceError::kKeyNotFound);
}

unexportable_keys::ServiceErrorOr<std::vector<uint8_t>>
FakeKeyedUnexportableKeyService::GetWrappedKey(
    unexportable_keys::UnexportableKeyId key_id) const {
  return base::unexpected(unexportable_keys::ServiceError::kKeyNotFound);
}

unexportable_keys::ServiceErrorOr<crypto::SignatureVerifier::SignatureAlgorithm>
FakeKeyedUnexportableKeyService::GetAlgorithm(
    unexportable_keys::UnexportableKeyId key_id) const {
  return base::unexpected(unexportable_keys::ServiceError::kKeyNotFound);
}

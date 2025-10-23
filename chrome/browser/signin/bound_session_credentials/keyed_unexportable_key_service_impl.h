// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_KEYED_UNEXPORTABLE_KEY_SERVICE_IMPL_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_KEYED_UNEXPORTABLE_KEY_SERVICE_IMPL_H_

#include "chrome/browser/signin/bound_session_credentials/keyed_unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"

// This is a shim class to integrate
// unexportable_keys::UnexportableKeyServiceImpl into the
// KeyedUnexportableKeyService hierarchy.
class KeyedUnexportableKeyServiceImpl : public KeyedUnexportableKeyService {
 public:
  // `task_manager` must outlive `KeyedUnexportableKeyServiceImpl`.
  explicit KeyedUnexportableKeyServiceImpl(
      unexportable_keys::UnexportableKeyTaskManager& task_manager);

  ~KeyedUnexportableKeyServiceImpl() override;

  // UnexportableKeyService:
  void GenerateSigningKeySlowlyAsync(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      unexportable_keys::BackgroundTaskPriority priority,
      base::OnceCallback<void(unexportable_keys::ServiceErrorOr<
                              unexportable_keys::UnexportableKeyId>)> callback)
      override;
  void FromWrappedSigningKeySlowlyAsync(
      base::span<const uint8_t> wrapped_key,
      unexportable_keys::BackgroundTaskPriority priority,
      base::OnceCallback<void(unexportable_keys::ServiceErrorOr<
                              unexportable_keys::UnexportableKeyId>)> callback)
      override;
  void SignSlowlyAsync(
      const unexportable_keys::UnexportableKeyId& key_id,
      base::span<const uint8_t> data,
      unexportable_keys::BackgroundTaskPriority priority,
      base::OnceCallback<void(
          unexportable_keys::ServiceErrorOr<std::vector<uint8_t>>)> callback)
      override;
  unexportable_keys::ServiceErrorOr<std::vector<uint8_t>>
  GetSubjectPublicKeyInfo(
      unexportable_keys::UnexportableKeyId key_id) const override;
  unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> GetWrappedKey(
      unexportable_keys::UnexportableKeyId key_id) const override;
  unexportable_keys::ServiceErrorOr<
      crypto::SignatureVerifier::SignatureAlgorithm>
  GetAlgorithm(unexportable_keys::UnexportableKeyId key_id) const override;

 private:
  unexportable_keys::UnexportableKeyServiceImpl key_service_implementation_;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_KEYED_UNEXPORTABLE_KEY_SERVICE_IMPL_H_

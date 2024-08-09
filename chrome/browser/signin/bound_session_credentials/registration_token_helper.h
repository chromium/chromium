// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_REGISTRATION_TOKEN_HELPER_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_REGISTRATION_TOKEN_HELPER_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/signature_verifier.h"
#include "url/gurl.h"

namespace base {
class Time;
}

namespace unexportable_keys {
class UnexportableKeyService;
class UnexportableKeyLoader;
}  // namespace unexportable_keys

// Helper class for generating registration tokens to bind the key on the
// server.
//
// A single instance can be used to generate multiple registration tokens for
// the same binding key. To use different binding keys, create multiple class
// instances.
class RegistrationTokenHelper {
 public:
  struct Result {
    unexportable_keys::UnexportableKeyId binding_key_id;
    std::vector<uint8_t> wrapped_binding_key;
    std::string registration_token;

    Result(unexportable_keys::UnexportableKeyId binding_key_id,
           std::vector<uint8_t> wrapped_binding_key,
           std::string registration_token);

    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
    Result(Result&& other);
    Result& operator=(Result&& other);

    ~Result();
  };

  // `unexportable_key_service` must outlive `this`.
  // If `wrapped_binding_key_to_reuse_` is not empty, `this` will reuse an
  // existing binding key instead of generating a new one.
  // TODO(alexilin): support timeout.
  explicit RegistrationTokenHelper(
      unexportable_keys::UnexportableKeyService& unexportable_key_service,
      const std::vector<uint8_t>& wrapped_binding_key_to_reuse = {});

  RegistrationTokenHelper(const RegistrationTokenHelper&) = delete;
  RegistrationTokenHelper& operator=(const RegistrationTokenHelper&) = delete;

  virtual ~RegistrationTokenHelper();

  // Invokes `callback` with a `Result` containing a new binding key ID and a
  // corresponding registration token on success. Otherwise, invokes `callback`
  // with `std::nullopt`.
  // Virtual for testing.
  virtual void GenerateForSessionBinding(
      std::string_view challenge,
      const GURL& registration_url,
      base::OnceCallback<void(std::optional<Result>)> callback);
  virtual void GenerateForTokenBinding(
      std::string_view client_id,
      std::string_view auth_code,
      const GURL& registration_url,
      base::OnceCallback<void(std::optional<Result>)> callback);

 private:
  using HeaderAndPayloadGenerator =
      base::RepeatingCallback<std::optional<std::string>(
          crypto::SignatureVerifier::SignatureAlgorithm,
          base::span<const uint8_t>,
          base::Time)>;

  void CreateKeyLoaderIfNeeded();
  void SignHeaderAndPayload(
      HeaderAndPayloadGenerator header_and_payload_generator,
      base::OnceCallback<void(std::optional<Result>)> callback,
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
          binding_key);
  void CreateRegistrationToken(
      std::string_view header_and_payload,
      unexportable_keys::UnexportableKeyId binding_key,
      base::OnceCallback<void(std::optional<Result>)> callback,
      unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> signature);

  const raw_ref<unexportable_keys::UnexportableKeyService>
      unexportable_key_service_;
  std::unique_ptr<unexportable_keys::UnexportableKeyLoader> key_loader_;
  const std::vector<uint8_t> wrapped_binding_key_to_reuse_;
  base::WeakPtrFactory<RegistrationTokenHelper> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_REGISTRATION_TOKEN_HELPER_H_

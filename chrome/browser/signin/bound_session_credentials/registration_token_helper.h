// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_REGISTRATION_TOKEN_HELPER_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_REGISTRATION_TOKEN_HELPER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece_forward.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace unexportable_keys {
class UnexportableKeyService;
}

// Helper class for generating a new binding key and a registration token to
// bind the key on the server.
//
// To use this class, simply create its instance and invoke `Start()` method.
// The helper will return the result asynchronously through `callback`.
//
// This class is intended for one time use and must be destroyed after
// `callback` is called.
class RegistrationTokenHelper {
 public:
  struct Result {
    Result(unexportable_keys::UnexportableKeyId binding_key_id,
           std::vector<uint8_t> wrapped_binding_key,
           std::string registration_token);
    ~Result();

    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
    Result(Result&& other);
    Result& operator=(Result&& other);

    unexportable_keys::UnexportableKeyId binding_key_id;
    std::vector<uint8_t> wrapped_binding_key;
    std::string registration_token;
  };

  // `unexportable_key_service` must outlive `this`.
  // Invokes `callback` with a `Result` containing a new binding key ID and a
  // corresponding registration token on success. Otherwise, invokes `callback`
  // with `absl::nullopt`.
  // TODO(alexilin): support timeout.
  explicit RegistrationTokenHelper(
      unexportable_keys::UnexportableKeyService& unexportable_key_service,
      base::StringPiece client_id,
      base::StringPiece auth_code,
      const GURL& registration_url,
      base::OnceCallback<void(absl::optional<Result>)> callback);

  RegistrationTokenHelper(const RegistrationTokenHelper&) = delete;
  RegistrationTokenHelper& operator=(const RegistrationTokenHelper&) = delete;

  virtual ~RegistrationTokenHelper();

  // virtual for testing.
  virtual void Start();

 private:
  // Callback for `GenerateSigningKeySlowlyAsync()`.
  void OnKeyGenerated(
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
          result);

  // Callback for `SignSlowlyAsync()`.
  void OnDataSigned(
      unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> result);

  const raw_ref<unexportable_keys::UnexportableKeyService>
      unexportable_key_service_;
  const std::string client_id_;
  const std::string auth_code_;
  const GURL registration_url_;
  base::OnceCallback<void(absl::optional<Result>)> callback_;

  bool started_ = false;
  unexportable_keys::UnexportableKeyId key_id_;
  std::string header_and_payload_;

  base::WeakPtrFactory<RegistrationTokenHelper> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_REGISTRATION_TOKEN_HELPER_H_

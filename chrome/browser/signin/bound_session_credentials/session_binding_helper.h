// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_SESSION_BINDING_HELPER_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_SESSION_BINDING_HELPER_H_

#include <cstdint>
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "base/strings/string_piece.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"

namespace unexportable_keys {
class UnexportableKeyService;
class UnexportableKeyLoader;
}  // namespace unexportable_keys

class GURL;

// `SessionBindingHelper` loads in-memory session binding key
// and provides an asynchronous method of creating a binding key assertion.
class SessionBindingHelper {
 public:
  SessionBindingHelper(
      unexportable_keys::UnexportableKeyService& unexportable_key_service,
      base::span<const uint8_t> wrapped_binding_key,
      std::string session_id);

  SessionBindingHelper(const SessionBindingHelper&) = delete;
  SessionBindingHelper& operator=(const SessionBindingHelper&) = delete;

  ~SessionBindingHelper();

  // Asynchronously loads the `wrapped_binding_key_` if loading it hasn't
  // started yet.
  void MaybeLoadBindingKey();

  // Asynchronously generates a binding key assertion with a key associated with
  // `wrapped_binding_key` passed in the constructor. The result is returned
  // through `callback`. Returns an empty string if the generation fails.
  void GenerateBindingKeyAssertion(
      base::StringPiece challenge,
      const GURL& destination_url,
      base::OnceCallback<void(std::string)> callback);

 private:
  friend class BoundSessionCookieControllerImplTest;
  FRIEND_TEST_ALL_PREFIXES(SessionBindingHelperTest, MaybeLoadBindingKey);

  void SignAssertionToken(
      base::StringPiece challenge,
      const GURL& destination_url,
      base::OnceCallback<void(std::string)> callback,
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
          binding_key);

  const raw_ref<unexportable_keys::UnexportableKeyService>
      unexportable_key_service_;
  const std::vector<uint8_t> wrapped_binding_key_;
  const std::string session_id_;

  std::unique_ptr<unexportable_keys::UnexportableKeyLoader> key_loader_;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_SESSION_BINDING_HELPER_H_

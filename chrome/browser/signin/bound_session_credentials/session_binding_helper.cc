// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/session_binding_helper.h"

#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/signin/public/base/session_binding_utils.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_loader.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "crypto/signature_verifier.h"
#include "url/gurl.h"

namespace {

constexpr std::string_view kSessionBindingNamespace = "CookieBinding";

unexportable_keys::BackgroundTaskPriority kSessionBindingPriority =
    unexportable_keys::BackgroundTaskPriority::kUserBlocking;

bool ShouldTryToReloadKey(
    const unexportable_keys::ServiceErrorOr<
        unexportable_keys::UnexportableKeyId>& key_id_or_error) {
  if (key_id_or_error.has_value()) {
    // The key was successfully loaded, no need to reload.
    return false;
  }
  unexportable_keys::ServiceError error = key_id_or_error.error();
  return error == unexportable_keys::ServiceError::kCryptoApiFailed ||
         error == unexportable_keys::ServiceError::kKeyCollision;
}

base::expected<std::string, SessionBindingHelper::Error> CreateAssertionToken(
    const std::string& header_and_payload,
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    std::vector<uint8_t> public_key,
    unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> signature) {
  using enum SessionBindingHelper::Error;
  if (!signature.has_value()) {
    return base::unexpected(kSignAssertionFailure);
  }

  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(algorithm, *signature, public_key)) {
    return base::unexpected(kVerifySignatureFailure);
  }
  verifier.VerifyUpdate(base::as_bytes(base::make_span(header_and_payload)));
  if (!verifier.VerifyFinal()) {
    return base::unexpected(kVerifySignatureFailure);
  }

  std::optional<std::string> assertion_token =
      signin::AppendSignatureToHeaderAndPayload(header_and_payload, algorithm,
                                                *signature);
  if (!assertion_token) {
    return base::unexpected(kAppendSignatureFailure);
  }

  return *assertion_token;
}
}  // namespace

SessionBindingHelper::SessionBindingHelper(
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    base::span<const uint8_t> wrapped_binding_key,
    std::string session_id)
    : unexportable_key_service_(unexportable_key_service),
      wrapped_binding_key_(wrapped_binding_key.begin(),
                           wrapped_binding_key.end()),
      session_id_(std::move(session_id)) {}

SessionBindingHelper::~SessionBindingHelper() = default;

void SessionBindingHelper::MaybeLoadBindingKey() {
  if (!key_loader_ || ShouldTryToReloadKey(key_loader_->GetKeyIdOrError())) {
    key_loader_ =
        unexportable_keys::UnexportableKeyLoader::CreateFromWrappedKey(
            unexportable_key_service_.get(), wrapped_binding_key_,
            kSessionBindingPriority);
  }
}

void SessionBindingHelper::GenerateBindingKeyAssertion(
    std::string_view challenge,
    const GURL& destination_url,
    base::OnceCallback<void(base::expected<std::string, Error>)> callback) {
  MaybeLoadBindingKey();
  // `base::Unretained(this)` is safe because `this` owns the
  // `UnexportableKeyLoader`.
  key_loader_->InvokeCallbackAfterKeyLoaded(base::BindOnce(
      &SessionBindingHelper::SignAssertionToken, base::Unretained(this),
      std::string(challenge), destination_url, std::move(callback)));
}

void SessionBindingHelper::SignAssertionToken(
    std::string_view challenge,
    const GURL& destination_url,
    base::OnceCallback<void(base::expected<std::string, Error>)> callback,
    unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
        binding_key) {
  if (!binding_key.has_value()) {
    std::move(callback).Run(base::unexpected(Error::kLoadKeyFailure));
    return;
  }

  crypto::SignatureVerifier::SignatureAlgorithm algorithm =
      *unexportable_key_service_->GetAlgorithm(*binding_key);
  std::vector<uint8_t> public_key =
      *unexportable_key_service_->GetSubjectPublicKeyInfo(*binding_key);
  std::optional<std::string> header_and_payload =
      signin::CreateKeyAssertionHeaderAndPayload(
          algorithm, public_key, session_id_, challenge, destination_url,
          kSessionBindingNamespace, /*ephemeral_key=*/nullptr);

  if (!header_and_payload.has_value()) {
    std::move(callback).Run(base::unexpected(Error::kCreateAssertionFailure));
    return;
  }

  unexportable_key_service_->SignSlowlyAsync(
      *binding_key, base::as_bytes(base::make_span(*header_and_payload)),
      kSessionBindingPriority,
      base::BindOnce(&CreateAssertionToken, *header_and_payload, algorithm,
                     std::move(public_key))
          .Then(std::move(callback)));
}

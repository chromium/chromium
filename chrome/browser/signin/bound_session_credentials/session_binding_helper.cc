// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/session_binding_helper.h"

#include <string>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/signin/public/base/session_binding_utils.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_loader.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "url/gurl.h"

namespace {

unexportable_keys::BackgroundTaskPriority kSessionBindingPriority =
    unexportable_keys::BackgroundTaskPriority::kUserBlocking;

std::string CreateAssertionToken(
    const std::string& header_and_payload,
    unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> signature) {
  if (!signature.has_value()) {
    return std::string();
  }

  return signin::AppendSignatureToHeaderAndPayload(header_and_payload,
                                                   *signature);
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
  if (!key_loader_) {
    key_loader_ =
        unexportable_keys::UnexportableKeyLoader::CreateFromWrappedKey(
            unexportable_key_service_.get(), wrapped_binding_key_,
            kSessionBindingPriority);
  }
}

void SessionBindingHelper::GenerateBindingKeyAssertion(
    base::StringPiece challenge,
    const GURL& destination_url,
    base::OnceCallback<void(std::string)> callback) {
  MaybeLoadBindingKey();
  // `base::Unretained(this)` is safe because `this` owns the
  // `UnexportableKeyLoader`.
  key_loader_->InvokeCallbackAfterKeyLoaded(base::BindOnce(
      &SessionBindingHelper::SignAssertionToken, base::Unretained(this),
      std::string(challenge), destination_url, std::move(callback)));
}

void SessionBindingHelper::SignAssertionToken(
    base::StringPiece challenge,
    const GURL& destination_url,
    base::OnceCallback<void(std::string)> callback,
    unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
        binding_key) {
  if (!binding_key.has_value()) {
    std::move(callback).Run(std::string());
    return;
  }

  absl::optional<std::string> header_and_payload =
      signin::CreateKeyAssertionHeaderAndPayload(
          *unexportable_key_service_->GetAlgorithm(*binding_key),
          *unexportable_key_service_->GetSubjectPublicKeyInfo(*binding_key),
          session_id_, challenge, destination_url);

  if (!header_and_payload.has_value()) {
    std::move(callback).Run(std::string());
    return;
  }

  unexportable_key_service_->SignSlowlyAsync(
      *binding_key, base::as_bytes(base::make_span(*header_and_payload)),
      kSessionBindingPriority,
      base::BindOnce(&CreateAssertionToken, *header_and_payload)
          .Then(std::move(callback)));
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/registration_token_helper.h"

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "components/signin/public/base/session_binding_utils.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "crypto/signature_verifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

// A server will provide a list of acceptable algorithms in the future.
constexpr crypto::SignatureVerifier::SignatureAlgorithm
    kAcceptableAlgorithms[] = {crypto::SignatureVerifier::ECDSA_SHA256,
                               crypto::SignatureVerifier::RSA_PKCS1_SHA256};

// New session registration doesn't block the user and can be done with a delay.
constexpr unexportable_keys::BackgroundTaskPriority kTaskPriority =
    unexportable_keys::BackgroundTaskPriority::kBestEffort;

}  // namespace

RegistrationTokenHelper::RegistrationTokenHelper(
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    base::StringPiece client_id,
    base::StringPiece auth_code,
    const GURL& registration_url,
    base::OnceCallback<void(absl::optional<Result>)> callback)
    : unexportable_key_service_(unexportable_key_service),
      client_id_(client_id),
      auth_code_(auth_code),
      registration_url_(registration_url),
      callback_(std::move(callback)) {}

RegistrationTokenHelper::~RegistrationTokenHelper() = default;

void RegistrationTokenHelper::Start() {
  CHECK(!started_);
  started_ = true;
  unexportable_key_service_->GenerateSigningKeySlowlyAsync(
      kAcceptableAlgorithms, kTaskPriority,
      base::BindOnce(&RegistrationTokenHelper::OnKeyGenerated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RegistrationTokenHelper::OnKeyGenerated(
    unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
        result) {
  if (!result.has_value()) {
    // TODO(alexilin): Record a histogram.
    std::move(callback_).Run(absl::nullopt);
    return;
  }
  key_id_ = *result;

  absl::optional<std::string> header_and_payload =
      signin::CreateKeyRegistrationHeaderAndPayload(
          *unexportable_key_service_->GetAlgorithm(key_id_),
          *unexportable_key_service_->GetSubjectPublicKeyInfo(key_id_),
          client_id_, auth_code_, registration_url_, base::Time::Now());

  if (!header_and_payload.has_value()) {
    // TODO(alexilin): Record a histogram.
    std::move(callback_).Run(absl::nullopt);
    return;
  }
  header_and_payload_ = std::move(*header_and_payload);

  unexportable_key_service_->SignSlowlyAsync(
      key_id_, base::as_bytes(base::make_span(header_and_payload_)),
      kTaskPriority,
      base::BindOnce(&RegistrationTokenHelper::OnDataSigned,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RegistrationTokenHelper::OnDataSigned(
    unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> result) {
  if (!result.has_value()) {
    // TODO(alexilin): Record a histogram.
    std::move(callback_).Run(absl::nullopt);
    return;
  }
  const std::vector<uint8_t>& signature = *result;
  std::string registration_token =
      signin::AppendSignatureToHeaderAndPayload(header_and_payload_, signature);

  std::move(callback_).Run(
      Result{.binding_key_id = key_id_,
             .registration_token = std::move(registration_token)});
}

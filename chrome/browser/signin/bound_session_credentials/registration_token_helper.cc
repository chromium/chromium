// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/registration_token_helper.h"

#include <optional>
#include <string_view>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "components/signin/public/base/session_binding_utils.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/unexportable_key_loader.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "crypto/signature_verifier.h"

namespace {

// New session registration doesn't block the user and can be done with a delay.
constexpr unexportable_keys::BackgroundTaskPriority kTaskPriority =
    unexportable_keys::BackgroundTaskPriority::kBestEffort;

}  // namespace

RegistrationTokenHelper::Result::Result(
    unexportable_keys::UnexportableKeyId in_binding_key_id,
    std::vector<uint8_t> in_wrapped_binding_key,
    std::string in_registration_token)
    : binding_key_id(in_binding_key_id),
      wrapped_binding_key(std::move(in_wrapped_binding_key)),
      registration_token(std::move(in_registration_token)) {}

RegistrationTokenHelper::Result::Result(Result&& other) = default;
RegistrationTokenHelper::Result& RegistrationTokenHelper::Result::operator=(
    Result&& other) = default;

RegistrationTokenHelper::Result::~Result() = default;

RegistrationTokenHelper::RegistrationTokenHelper(
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    KeyInitParam key_init_param)
    : unexportable_key_service_(unexportable_key_service),
      key_init_param_(std::move(key_init_param)) {}

RegistrationTokenHelper::~RegistrationTokenHelper() = default;

void RegistrationTokenHelper::GenerateForSessionBinding(
    std::string_view challenge,
    const GURL& registration_url,
    base::OnceCallback<void(std::optional<Result>)> callback) {
  CreateKeyLoaderIfNeeded();
  HeaderAndPayloadGenerator header_and_payload_generator = base::BindRepeating(
      &signin::CreateKeyRegistrationHeaderAndPayloadForSessionBinding,
      std::string(challenge), registration_url);
  key_loader_->InvokeCallbackAfterKeyLoaded(base::BindOnce(
      &RegistrationTokenHelper::SignHeaderAndPayload,
      weak_ptr_factory_.GetWeakPtr(), std::move(header_and_payload_generator),
      std::move(callback)));
}
void RegistrationTokenHelper::GenerateForTokenBinding(
    std::string_view client_id,
    std::string_view auth_code,
    const GURL& registration_url,
    base::OnceCallback<void(std::optional<Result>)> callback) {
  CreateKeyLoaderIfNeeded();
  HeaderAndPayloadGenerator header_and_payload_generator = base::BindRepeating(
      &signin::CreateKeyRegistrationHeaderAndPayloadForTokenBinding,
      std::string(client_id), std::string(auth_code), registration_url);
  key_loader_->InvokeCallbackAfterKeyLoaded(base::BindOnce(
      &RegistrationTokenHelper::SignHeaderAndPayload,
      weak_ptr_factory_.GetWeakPtr(), std::move(header_and_payload_generator),
      std::move(callback)));
}

void RegistrationTokenHelper::CreateKeyLoaderIfNeeded() {
  if (key_loader_) {
    return;
  }

  std::visit(
      base::Overloaded{
          [&](const std::vector<uint8_t>& wrapped_binding_key_to_reuse) {
            key_loader_ =
                unexportable_keys::UnexportableKeyLoader::CreateFromWrappedKey(
                    unexportable_key_service_.get(),
                    wrapped_binding_key_to_reuse, kTaskPriority);
          },
          [&](const std::vector<crypto::SignatureVerifier::SignatureAlgorithm>&
                  acceptable_algorithms) {
            key_loader_ =
                unexportable_keys::UnexportableKeyLoader::CreateWithNewKey(
                    unexportable_key_service_.get(), acceptable_algorithms,
                    kTaskPriority);
          }},
      key_init_param_);
}

void RegistrationTokenHelper::SignHeaderAndPayload(
    HeaderAndPayloadGenerator header_and_payload_generator,
    base::OnceCallback<void(std::optional<Result>)> callback,
    unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
        binding_key) {
  if (!binding_key.has_value()) {
    // TODO(alexilin): Record a histogram.
    std::move(callback).Run(std::nullopt);
    return;
  }

  crypto::SignatureVerifier::SignatureAlgorithm algorithm =
      *unexportable_key_service_->GetAlgorithm(*binding_key);
  std::optional<std::string> header_and_payload =
      header_and_payload_generator.Run(
          algorithm,
          *unexportable_key_service_->GetSubjectPublicKeyInfo(*binding_key),
          base::Time::Now());

  if (!header_and_payload.has_value()) {
    // TODO(alexilin): Record a histogram.
    std::move(callback).Run(std::nullopt);
    return;
  }

  unexportable_key_service_->SignSlowlyAsync(
      *binding_key, base::as_bytes(base::make_span(*header_and_payload)),
      kTaskPriority,
      base::BindOnce(&RegistrationTokenHelper::CreateRegistrationToken,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::string(*header_and_payload), *binding_key,
                     std::move(callback)));
}

void RegistrationTokenHelper::CreateRegistrationToken(
    std::string_view header_and_payload,
    unexportable_keys::UnexportableKeyId binding_key,
    base::OnceCallback<void(std::optional<Result>)> callback,
    unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> signature) {
  if (!signature.has_value()) {
    // TODO(alexilin): Record a histogram.
    std::move(callback).Run(std::nullopt);
    return;
  }

  crypto::SignatureVerifier::SignatureAlgorithm algorithm =
      *unexportable_key_service_->GetAlgorithm(binding_key);
  std::optional<std::string> registration_token =
      signin::AppendSignatureToHeaderAndPayload(header_and_payload, algorithm,
                                                *signature);
  if (!registration_token.has_value()) {
    // TODO(alexilin): Record a histogram.
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::vector<uint8_t> wrapped_key =
      *unexportable_key_service_->GetWrappedKey(binding_key);
  std::move(callback).Run(Result(binding_key, std::move(wrapped_key),
                                 std::move(registration_token).value()));
}

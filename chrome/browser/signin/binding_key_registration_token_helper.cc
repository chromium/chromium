// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/binding_key_registration_token_helper.h"

#include <optional>
#include <string_view>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/optional_util.h"
#include "components/signin/public/base/session_binding_utils.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/unexportable_key_loader.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "crypto/signature_verifier.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace {

constexpr std::string_view kTokenBindingResultHistogram =
    "Signin.TokenBinding.GenerateRegistrationTokenResult";
constexpr std::string_view kSessionBindingResultHistogram =
    "Signin.BoundSessionCredentials."
    "SessionRegistrationGenerateRegistrationTokenResult";

// New session registration doesn't block the user and can be done with a delay.
constexpr unexportable_keys::BackgroundTaskPriority kTaskPriority =
    unexportable_keys::BackgroundTaskPriority::kBestEffort;

}  // namespace

BindingKeyRegistrationTokenHelper::Result::Result(
    unexportable_keys::UnexportableKeyId in_binding_key_id,
    std::vector<uint8_t> in_wrapped_binding_key,
    std::string in_registration_token)
    : binding_key_id(in_binding_key_id),
      wrapped_binding_key(std::move(in_wrapped_binding_key)),
      registration_token(std::move(in_registration_token)) {}

BindingKeyRegistrationTokenHelper::Result::Result(Result&& other) = default;
BindingKeyRegistrationTokenHelper::Result&
BindingKeyRegistrationTokenHelper::Result::operator=(Result&& other) = default;

BindingKeyRegistrationTokenHelper::Result::~Result() = default;

BindingKeyRegistrationTokenHelper::BindingKeyRegistrationTokenHelper(
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    KeyInitParam key_init_param)
    : unexportable_key_service_(unexportable_key_service),
      key_init_param_(std::move(key_init_param)) {}

BindingKeyRegistrationTokenHelper::~BindingKeyRegistrationTokenHelper() = default;

void BindingKeyRegistrationTokenHelper::GenerateForSessionBinding(
    std::string_view challenge,
    const GURL& registration_url,
    base::OnceCallback<void(std::optional<Result>)> callback) {
  CreateKeyLoaderIfNeeded();
  HeaderAndPayloadGenerator header_and_payload_generator = base::BindRepeating(
      &signin::CreateKeyRegistrationHeaderAndPayloadForSessionBinding,
      std::string(challenge), registration_url);
  key_loader_->InvokeCallbackAfterKeyLoaded(
      base::BindOnce(&BindingKeyRegistrationTokenHelper::SignHeaderAndPayload,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(header_and_payload_generator),
                     base::BindOnce(&BindingKeyRegistrationTokenHelper::
                                        RecordResultAndInvokeCallback,
                                    kSessionBindingResultHistogram,
                                    std::move(callback))));
}
void BindingKeyRegistrationTokenHelper::GenerateForTokenBinding(
    std::string_view client_id,
    std::string_view auth_code,
    const GURL& registration_url,
    base::OnceCallback<void(std::optional<Result>)> callback) {
  CreateKeyLoaderIfNeeded();
  HeaderAndPayloadGenerator header_and_payload_generator = base::BindRepeating(
      &signin::CreateKeyRegistrationHeaderAndPayloadForTokenBinding,
      std::string(client_id), std::string(auth_code), registration_url);
  key_loader_->InvokeCallbackAfterKeyLoaded(
      base::BindOnce(&BindingKeyRegistrationTokenHelper::SignHeaderAndPayload,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(header_and_payload_generator),
                     base::BindOnce(&BindingKeyRegistrationTokenHelper::
                                        RecordResultAndInvokeCallback,
                                    kTokenBindingResultHistogram,
                                    std::move(callback))));
}

void BindingKeyRegistrationTokenHelper::CreateKeyLoaderIfNeeded() {
  if (key_loader_) {
    return;
  }

  std::visit(
      absl::Overload{
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

void BindingKeyRegistrationTokenHelper::SignHeaderAndPayload(
    HeaderAndPayloadGenerator header_and_payload_generator,
    base::OnceCallback<void(base::expected<Result, Error>)> callback,
    unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
        binding_key) {
  if (!binding_key.has_value()) {
    Error error = std::visit(
        absl::Overload{[](const std::vector<uint8_t>&) {
                         return Error::kLoadReusedKeyFailure;
                       },
                       [](const std::vector<
                           crypto::SignatureVerifier::SignatureAlgorithm>&) {
                         return Error::kGenerateNewKeyFailure;
                       }},
        key_init_param_);
    std::move(callback).Run(base::unexpected(error));
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
    std::move(callback).Run(base::unexpected(Error::kCreateAssertionFailure));
    return;
  }

  unexportable_key_service_->SignSlowlyAsync(
      *binding_key, base::as_byte_span(*header_and_payload), kTaskPriority,
      base::BindOnce(
          &BindingKeyRegistrationTokenHelper::CreateRegistrationToken,
          weak_ptr_factory_.GetWeakPtr(), std::string(*header_and_payload),
          *binding_key, std::move(callback)));
}

void BindingKeyRegistrationTokenHelper::CreateRegistrationToken(
    std::string_view header_and_payload,
    unexportable_keys::UnexportableKeyId binding_key,
    base::OnceCallback<void(base::expected<Result, Error>)> callback,
    unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> signature) {
  if (!signature.has_value()) {
    std::move(callback).Run(base::unexpected(Error::kSignAssertionFailure));
    return;
  }

  crypto::SignatureVerifier::SignatureAlgorithm algorithm =
      *unexportable_key_service_->GetAlgorithm(binding_key);
  std::vector<uint8_t> pubkey =
      *unexportable_key_service_->GetSubjectPublicKeyInfo(binding_key);
  std::optional<std::string> registration_token =
      signin::AppendSignatureToHeaderAndPayload(header_and_payload, algorithm,
                                                pubkey, *signature);
  if (!registration_token.has_value()) {
    std::move(callback).Run(base::unexpected(Error::kAppendSignatureFailure));
    return;
  }

  std::vector<uint8_t> wrapped_key =
      *unexportable_key_service_->GetWrappedKey(binding_key);
  std::move(callback).Run(Result(binding_key, std::move(wrapped_key),
                                 std::move(registration_token).value()));
}

void BindingKeyRegistrationTokenHelper::RecordResultAndInvokeCallback(
    std::string_view result_histogram_name,
    base::OnceCallback<
        void(std::optional<BindingKeyRegistrationTokenHelper::Result>)>
        callback,
    base::expected<BindingKeyRegistrationTokenHelper::Result,
                   BindingKeyRegistrationTokenHelper::Error>
        result_or_error) {
  base::UmaHistogramEnumeration(result_histogram_name,
                                result_or_error.error_or(Error::kNone));
  std::move(callback).Run(
      base::OptionalFromExpected(std::move(result_or_error)));
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/passkeys/passkey_authenticator_service_ash.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "chromeos/crosapi/mojom/passkeys.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_utils.h"

namespace ash {

PasskeyAuthenticatorServiceAsh::CreateRequestContext::CreateRequestContext() =
    default;

PasskeyAuthenticatorServiceAsh::CreateRequestContext::CreateRequestContext(
    CreateRequestContext&&) = default;

PasskeyAuthenticatorServiceAsh::CreateRequestContext&
PasskeyAuthenticatorServiceAsh::CreateRequestContext::operator=(
    CreateRequestContext&&) = default;

PasskeyAuthenticatorServiceAsh::CreateRequestContext::~CreateRequestContext() =
    default;

PasskeyAuthenticatorServiceAsh::AssertRequestContext::AssertRequestContext() =
    default;

PasskeyAuthenticatorServiceAsh::AssertRequestContext::AssertRequestContext(
    AssertRequestContext&&) = default;

PasskeyAuthenticatorServiceAsh::AssertRequestContext&
PasskeyAuthenticatorServiceAsh::AssertRequestContext::operator=(
    AssertRequestContext&&) = default;

PasskeyAuthenticatorServiceAsh::AssertRequestContext::~AssertRequestContext() =
    default;

PasskeyAuthenticatorServiceAsh::PasskeyAuthenticatorServiceAsh(
    CoreAccountInfo account_info,
    webauthn::PasskeyModel* passkey_model,
    trusted_vault::TrustedVaultClient* trusted_vault_client)
    : primary_account_info_(std::move(account_info)),
      passkey_model_(passkey_model),
      trusted_vault_client_(trusted_vault_client) {}

PasskeyAuthenticatorServiceAsh::~PasskeyAuthenticatorServiceAsh() = default;

void PasskeyAuthenticatorServiceAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::PasskeyAuthenticator>
        pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void PasskeyAuthenticatorServiceAsh::Create(
    crosapi::mojom::AccountKeyPtr account_key,
    crosapi::mojom::PasskeyCreationRequestPtr request,
    CreateCallback callback) {
  if (!IsPrimaryAccount(account_key)) {
    std::move(callback).Run(crosapi::mojom::PasskeyCreationResult::NewError(
        crosapi::mojom::PasskeyCreationError::kNonPrimaryAccount));
    return;
  }

  if (processing_request_) {
    std::move(callback).Run(crosapi::mojom::PasskeyCreationResult::NewError(
        crosapi::mojom::PasskeyCreationError::kPendingRequest));
    return;
  }

  processing_request_ = true;

  CreateRequestContext ctx;
  ctx.request = std::move(request);
  ctx.pending_callback = std::move(callback);
  FetchTrustedVaultKeys(
      base::BindOnce(&PasskeyAuthenticatorServiceAsh::DoCreate,
                     weak_factory_.GetWeakPtr(), std::move(ctx)));
}

void PasskeyAuthenticatorServiceAsh::Assert(
    crosapi::mojom::AccountKeyPtr account_key,
    crosapi::mojom::PasskeyAssertionRequestPtr request,
    AssertCallback callback) {
  if (!IsPrimaryAccount(account_key)) {
    std::move(callback).Run(crosapi::mojom::PasskeyAssertionResult::NewError(
        crosapi::mojom::PasskeyAssertionError::kNonPrimaryAccount));
    return;
  }

  if (processing_request_) {
    std::move(callback).Run(crosapi::mojom::PasskeyAssertionResult::NewError(
        crosapi::mojom::PasskeyAssertionError::kPendingRequest));
    return;
  }

  processing_request_ = true;

  AssertRequestContext ctx;
  ctx.request = std::move(request);
  ctx.pending_callback = std::move(callback);
  FetchTrustedVaultKeys(
      base::BindOnce(&PasskeyAuthenticatorServiceAsh::DoAssert,
                     weak_factory_.GetWeakPtr(), std::move(ctx)));
}

void PasskeyAuthenticatorServiceAsh::FetchTrustedVaultKeys(
    SecurityDomainSecretCallback callback) {
  trusted_vault_client_->FetchKeys(
      primary_account_info_,
      base::BindOnce(&PasskeyAuthenticatorServiceAsh::OnHaveTrustedVaultKeys,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void PasskeyAuthenticatorServiceAsh::OnHaveTrustedVaultKeys(
    SecurityDomainSecretCallback callback,
    const std::vector<std::vector<uint8_t>>& keys) {
  if (keys.empty()) {
    // TODO(crbug.com/40187814): Implement security domain secret recovery UI
    // flow.
    NOTIMPLEMENTED();
    return std::move(callback).Run(std::nullopt);
  }
  return std::move(callback).Run(keys.back());
}

void PasskeyAuthenticatorServiceAsh::DoCreate(
    CreateRequestContext ctx,
    std::optional<std::vector<uint8_t>> security_domain_secret) {
  if (!security_domain_secret) {
    FinishCreate(std::move(ctx),
                 crosapi::mojom::PasskeyCreationResult::NewError(
                     crosapi::mojom::PasskeyCreationError::
                         kSecurityDomainSecretUnavailable));
    return;
  }

  // TODO(crbug.com/40187814): Implement user verification.

  // TODO(crbug.com/40187814): Get the epoch/version of the security domain
  // secret and pass into CreatePasskey().
  std::vector<uint8_t> public_key_spki_der;
  sync_pb::WebauthnCredentialSpecifics passkey = passkey_model_->CreatePasskey(
      ctx.request->rp_id,
      webauthn::PasskeyModel::UserEntity(ctx.request->user_id,
                                         ctx.request->user_name,
                                         ctx.request->user_display_name),
      *security_domain_secret,
      /*trusted_vault_key_version=*/0, &public_key_spki_der);

  auto response = crosapi::mojom::PasskeyCreationResponse::New();
  response->authenticator_data =
      webauthn::passkey_model_utils::MakeAuthenticatorDataForCreation(
          ctx.request->rp_id, base::as_byte_span(passkey.credential_id()),
          public_key_spki_der);

  FinishCreate(
      std::move(ctx),
      crosapi::mojom::PasskeyCreationResult::NewResponse(std::move(response)));
}

void PasskeyAuthenticatorServiceAsh::DoAssert(
    AssertRequestContext ctx,
    std::optional<std::vector<uint8_t>> security_domain_secret) {
  if (!security_domain_secret) {
    FinishAssert(std::move(ctx),
                 crosapi::mojom::PasskeyAssertionResult::NewError(
                     crosapi::mojom::PasskeyAssertionError::
                         kSecurityDomainSecretUnavailable));
    return;
  }

  const std::string credential_id(ctx.request->credential_id.begin(),
                                  ctx.request->credential_id.end());
  std::optional<sync_pb::WebauthnCredentialSpecifics> credential_specifics =
      passkey_model_->GetPasskeyByCredentialId(ctx.request->rp_id,
                                               credential_id);
  if (!credential_specifics) {
    FinishAssert(
        std::move(ctx),
        crosapi::mojom::PasskeyAssertionResult::NewError(
            crosapi::mojom::PasskeyAssertionError::kCredentialNotFound));
    return;
  }

  sync_pb::WebauthnCredentialSpecifics_Encrypted credential_secrets;
  if (!webauthn::passkey_model_utils::DecryptWebauthnCredentialSpecificsData(
          *security_domain_secret, *credential_specifics,
          &credential_secrets)) {
    FinishAssert(std::move(ctx),
                 crosapi::mojom::PasskeyAssertionResult::NewError(
                     crosapi::mojom::PasskeyAssertionError::
                         kSecurityDomainSecretUnavailable));
    return;
  }

  // TODO(crbug.com/40187814): Implement user verification.

  std::vector<uint8_t> authenticator_data =
      webauthn::passkey_model_utils::MakeAuthenticatorDataForAssertion(
          ctx.request->rp_id);
  std::vector<uint8_t> signed_over_data(authenticator_data);
  signed_over_data.insert(signed_over_data.end(),
                          ctx.request->client_data_hash.begin(),
                          ctx.request->client_data_hash.end());
  std::optional<std::vector<uint8_t>> assertion_signature =
      webauthn::passkey_model_utils::GenerateEcSignature(
          base::as_byte_span(credential_secrets.private_key()),
          signed_over_data);
  if (!assertion_signature) {
    FinishAssert(std::move(ctx),
                 crosapi::mojom::PasskeyAssertionResult::NewError(
                     crosapi::mojom::PasskeyAssertionError::kInternalError));
    return;
  }

  auto response = crosapi::mojom::PasskeyAssertionResponse::New();
  response->signature = std::move(*assertion_signature);
  response->authenticator_data = std::move(authenticator_data);

  FinishAssert(
      std::move(ctx),
      crosapi::mojom::PasskeyAssertionResult::NewResponse(std::move(response)));
}

void PasskeyAuthenticatorServiceAsh::FinishCreate(
    CreateRequestContext ctx,
    crosapi::mojom::PasskeyCreationResultPtr result) {
  processing_request_ = false;
  std::move(ctx.pending_callback).Run(std::move(result));
}

void PasskeyAuthenticatorServiceAsh::FinishAssert(
    AssertRequestContext ctx,
    crosapi::mojom::PasskeyAssertionResultPtr result) {
  processing_request_ = false;
  std::move(ctx.pending_callback).Run(std::move(result));
}

bool PasskeyAuthenticatorServiceAsh::IsPrimaryAccount(
    const crosapi::mojom::AccountKeyPtr& mojo_account_key) const {
  const std::optional<account_manager::AccountKey> account_key =
      account_manager::FromMojoAccountKey(mojo_account_key);
  if (!account_key ||
      (account_key->account_type() != account_manager::AccountType::kGaia) ||
      account_key->id().empty()) {
    return false;
  }

  return account_key->id() == primary_account_info_.gaia;
}

}  // namespace ash

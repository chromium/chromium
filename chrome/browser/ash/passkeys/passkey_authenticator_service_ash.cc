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
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "crypto/sha2.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/p256_public_key.h"
#include "device/fido/public_key.h"

namespace ash {

namespace {

constexpr std::array<const uint8_t, 16> kGpmAaguid{
    0xea, 0x9b, 0x8d, 0x66, 0x4d, 0x01, 0x1d, 0x21,
    0x3c, 0xe4, 0xb6, 0xb4, 0x8c, 0xb5, 0x75, 0xd4};

// Returns the WebAuthn authenticator data for this authenticator. See
// https://w3c.github.io/webauthn/#authenticator-data.
std::vector<uint8_t> MakeAuthenticatorDataForAssertion(std::string_view rp_id) {
  using Flag = device::AuthenticatorData::Flag;
  return device::AuthenticatorData(
             crypto::SHA256Hash(base::as_byte_span(rp_id)),
             {Flag::kTestOfUserPresence, Flag::kTestOfUserVerification,
              Flag::kBackupEligible, Flag::kBackupState},
             /*sign_counter=*/0u,
             /*attested_credential_data=*/std::nullopt,
             /*extensions=*/std::nullopt)
      .SerializeToByteArray();
}

std::vector<uint8_t> MakeAuthenticatorDataForCreation(
    std::string_view rp_id,
    base::span<const uint8_t> credential_id,
    base::span<const uint8_t> public_key_spki_der) {
  using Flag = device::AuthenticatorData::Flag;
  std::unique_ptr<device::PublicKey> public_key =
      device::P256PublicKey::ParseSpkiDer(
          base::strict_cast<int32_t>(device::CoseAlgorithmIdentifier::kEs256),
          public_key_spki_der);
  device::AttestedCredentialData attested_credential_data(
      kGpmAaguid, credential_id, std::move(public_key));
  return device::AuthenticatorData(
             crypto::SHA256Hash(base::as_byte_span(rp_id)),
             {Flag::kTestOfUserPresence, Flag::kTestOfUserVerification,
              Flag::kBackupEligible, Flag::kBackupState, Flag::kAttestation},
             /*sign_counter=*/0u, std::move(attested_credential_data),
             /*extensions=*/std::nullopt)
      .SerializeToByteArray();
}

std::optional<std::vector<uint8_t>> GenerateEcSignature(
    base::span<const uint8_t> pkcs8_ec_private_key,
    base::span<const uint8_t> signed_over_data) {
  auto ec_private_key =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(pkcs8_ec_private_key);
  if (!ec_private_key) {
    return std::nullopt;
  }
  auto signer = crypto::ECSignatureCreator::Create(ec_private_key.get());
  std::vector<uint8_t> signature;
  if (!signer->Sign(signed_over_data, &signature)) {
    return std::nullopt;
  }
  return signature;
}

}  // namespace

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
    // TODO(crbug.com/1223853): Implement security domain secret recovery UI
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

  // TODO(crbug.com/1223853): Implement user verification.

  // TODO(crbug.com/1223853): Get the epoch/version of the security domain
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
  response->authenticator_data = MakeAuthenticatorDataForCreation(
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

  // TODO(crbug.com/1223853): Implement user verification.

  std::vector<uint8_t> authenticator_data =
      MakeAuthenticatorDataForAssertion(ctx.request->rp_id);
  std::vector<uint8_t> signed_over_data(authenticator_data);
  signed_over_data.insert(signed_over_data.end(),
                          ctx.request->client_data_hash.begin(),
                          ctx.request->client_data_hash.end());
  std::optional<std::vector<uint8_t>> assertion_signature = GenerateEcSignature(
      base::as_byte_span(credential_secrets.private_key()), signed_over_data);
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

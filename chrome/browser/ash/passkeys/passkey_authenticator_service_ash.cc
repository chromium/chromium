// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/passkeys/passkey_authenticator_service_ash.h"

#include "base/containers/span.h"
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
#include "device/fido/authenticator_data.h"

namespace ash {

namespace {

absl::optional<std::vector<uint8_t>> GenerateEcSignature(
    base::span<const uint8_t> pkcs8_ec_private_key,
    base::span<const uint8_t> signed_over_data) {
  auto ec_private_key =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(pkcs8_ec_private_key);
  if (!ec_private_key) {
    return absl::nullopt;
  }
  auto signer = crypto::ECSignatureCreator::Create(ec_private_key.get());
  std::vector<uint8_t> signature;
  if (!signer->Sign(signed_over_data, &signature)) {
    return absl::nullopt;
  }
  return signature;
}

}  // namespace

PasskeyAuthenticatorServiceAsh::RequestState::RequestState() = default;

PasskeyAuthenticatorServiceAsh::RequestState::~RequestState() = default;

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

void PasskeyAuthenticatorServiceAsh::Assert(
    crosapi::mojom::AccountKeyPtr account_key,
    crosapi::mojom::PasskeyAssertionRequestPtr request,
    AssertCallback callback) {
  if (!IsPrimaryAccount(account_key)) {
    std::move(callback).Run(crosapi::mojom::PasskeyAssertionResult::NewError(
        crosapi::mojom::PasskeyAssertionError::kNonPrimaryAccount));
    return;
  }

  if (request_state_) {
    std::move(callback).Run(crosapi::mojom::PasskeyAssertionResult::NewError(
        crosapi::mojom::PasskeyAssertionError::kPendingRequest));
    return;
  }
  request_state_.emplace();
  request_state_->pending_assert_callback = std::move(callback);
  request_state_->assert_request = std::move(request);
  FetchTrustedVaultKeys(base::BindOnce(
      &PasskeyAuthenticatorServiceAsh::DoAssert, weak_factory_.GetWeakPtr()));
}

void PasskeyAuthenticatorServiceAsh::FetchTrustedVaultKeys(
    base::OnceCallback<void()> callback) {
  trusted_vault_client_->FetchKeys(
      primary_account_info_,
      base::BindOnce(&PasskeyAuthenticatorServiceAsh::OnHaveTrustedVaultKeys,
                     weak_factory_.GetWeakPtr())
          .Then(std::move(callback)));
}

void PasskeyAuthenticatorServiceAsh::OnHaveTrustedVaultKeys(
    const std::vector<std::vector<uint8_t>>& keys) {
  if (keys.empty()) {
    // TODO(crbug.com/1223853): Implement security domain secret recovery UI
    // flow.
    NOTIMPLEMENTED();
    return;
  }
  request_state_->security_domain_secret = keys.back();
}

void PasskeyAuthenticatorServiceAsh::DoAssert() {
  CHECK(request_state_);

  if (!request_state_->security_domain_secret) {
    FinishAssert(crosapi::mojom::PasskeyAssertionResult::NewError(
        crosapi::mojom::PasskeyAssertionError::
            kSecurityDomainSecretUnavailable));
    return;
  }

  std::string credential_id(
      request_state_->assert_request->credential_id.begin(),
      request_state_->assert_request->credential_id.end());
  absl::optional<sync_pb::WebauthnCredentialSpecifics> credential_specifics =
      passkey_model_->GetPasskeyByCredentialId(
          request_state_->assert_request->rp_id, credential_id);
  if (!credential_specifics) {
    FinishAssert(crosapi::mojom::PasskeyAssertionResult::NewError(
        crosapi::mojom::PasskeyAssertionError::kCredentialNotFound));
    return;
  }

  sync_pb::WebauthnCredentialSpecifics_Encrypted credential_secrets;
  if (!webauthn::passkey_model_utils::DecryptWebauthnCredentialSpecificsData(
          *request_state_->security_domain_secret, *credential_specifics,
          &credential_secrets)) {
    FinishAssert(crosapi::mojom::PasskeyAssertionResult::NewError(
        crosapi::mojom::PasskeyAssertionError::
            kSecurityDomainSecretUnavailable));
    return;
  }

  // TODO(crbug.com/1223853): Implement user verification.

  device::AuthenticatorData authenticator_data(
      crypto::SHA256Hash(base::as_bytes(
          base::make_span(request_state_->assert_request->rp_id))),
      /*user_present=*/true,
      /*user_verified=*/true, /*backup_eligible=*/true, /*backup_state=*/false,
      /*sign_counter=*/0,
      /*attested_credential_data=*/absl::nullopt, /*extensions=*/absl::nullopt);
  std::vector<uint8_t> signed_over_data =
      authenticator_data.SerializeToByteArray();
  signed_over_data.insert(
      signed_over_data.end(),
      request_state_->assert_request->client_data_hash.begin(),
      request_state_->assert_request->client_data_hash.end());
  absl::optional<std::vector<uint8_t>> assertion_signature =
      GenerateEcSignature(
          base::as_bytes(base::make_span(credential_secrets.private_key())),
          signed_over_data);
  if (!assertion_signature) {
    FinishAssert(crosapi::mojom::PasskeyAssertionResult::NewError(
        crosapi::mojom::PasskeyAssertionError::kInternalError));
    return;
  }

  auto response = crosapi::mojom::PasskeyAssertionResponse::New();
  response->signature = std::move(*assertion_signature);

  FinishAssert(
      crosapi::mojom::PasskeyAssertionResult::NewResponse(std::move(response)));
}

void PasskeyAuthenticatorServiceAsh::FinishAssert(
    crosapi::mojom::PasskeyAssertionResultPtr result) {
  CHECK(request_state_);
  AssertCallback callback = std::move(request_state_->pending_assert_callback);
  request_state_ = absl::nullopt;
  std::move(callback).Run(std::move(result));
}

bool PasskeyAuthenticatorServiceAsh::IsPrimaryAccount(
    const crosapi::mojom::AccountKeyPtr& mojo_account_key) const {
  const absl::optional<account_manager::AccountKey> account_key =
      account_manager::FromMojoAccountKey(mojo_account_key);
  if (!account_key ||
      (account_key->account_type() != account_manager::AccountType::kGaia) ||
      account_key->id().empty()) {
    return false;
  }

  return account_key->id() == primary_account_info_.gaia;
}

}  // namespace ash

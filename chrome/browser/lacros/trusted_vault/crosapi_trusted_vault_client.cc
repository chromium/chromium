// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/trusted_vault/crosapi_trusted_vault_client.h"

#include <utility>

#include "base/functional/callback.h"
#include "chromeos/crosapi/mojom/account_manager.mojom-forward.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace {

crosapi::mojom::AccountKeyPtr MakeMojoAccountKey(const std::string& gaia_id) {
  return account_manager::ToMojoAccountKey(account_manager::AccountKey(
      gaia_id, account_manager::AccountType::kGaia));
}

}  // namespace

CrosapiTrustedVaultClient::CrosapiTrustedVaultClient(
    mojo::Remote<crosapi::mojom::TrustedVaultBackend>* remote)
    : remote_(remote) {
  CHECK(remote_);
  CHECK(remote_->is_bound());

  (*remote_)->AddObserver(receiver_.BindNewPipeAndPassRemote());
}

CrosapiTrustedVaultClient::~CrosapiTrustedVaultClient() = default;

void CrosapiTrustedVaultClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CrosapiTrustedVaultClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CrosapiTrustedVaultClient::FetchKeys(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)> cb) {
  (*remote_)->FetchKeys(MakeMojoAccountKey(account_info.gaia), std::move(cb));
}

void CrosapiTrustedVaultClient::StoreKeys(
    const std::string& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  (*remote_)->StoreKeys(MakeMojoAccountKey(gaia_id), keys, last_key_version);
}

void CrosapiTrustedVaultClient::MarkLocalKeysAsStale(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> cb) {
  (*remote_)->MarkLocalKeysAsStale(MakeMojoAccountKey(account_info.gaia),
                                   std::move(cb));
}

void CrosapiTrustedVaultClient::GetIsRecoverabilityDegraded(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> cb) {
  (*remote_)->GetIsRecoverabilityDegraded(MakeMojoAccountKey(account_info.gaia),
                                          std::move(cb));
}

void CrosapiTrustedVaultClient::AddTrustedRecoveryMethod(
    const std::string& gaia_id,
    const std::vector<uint8_t>& public_key,
    int method_type_hint,
    base::OnceClosure cb) {
  (*remote_)->AddTrustedRecoveryMethod(MakeMojoAccountKey(gaia_id), public_key,
                                       method_type_hint, std::move(cb));
}

void CrosapiTrustedVaultClient::ClearLocalDataForAccount(
    const CoreAccountInfo& account_info) {
  (*remote_)->ClearLocalDataForAccount(MakeMojoAccountKey(account_info.gaia));
}

void CrosapiTrustedVaultClient::OnTrustedVaultKeysChanged() {
  for (Observer& observer : observers_) {
    observer.OnTrustedVaultKeysChanged();
  }
}

void CrosapiTrustedVaultClient::OnTrustedVaultRecoverabilityChanged() {
  for (Observer& observer : observers_) {
    observer.OnTrustedVaultRecoverabilityChanged();
  }
}

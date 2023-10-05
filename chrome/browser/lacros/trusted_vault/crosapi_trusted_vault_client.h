// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_TRUSTED_VAULT_CROSAPI_TRUSTED_VAULT_CLIENT_H_
#define CHROME_BROWSER_LACROS_TRUSTED_VAULT_CROSAPI_TRUSTED_VAULT_CLIENT_H_

#include "base/observer_list.h"
#include "chromeos/crosapi/mojom/trusted_vault.mojom.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

// Implements TrustedVaultClient interface by plumbing calls to Ash-side
// TrustedVaultClient via Crosapi.
// TODO(crbug.com/1434667): Add coverage by browser tests.
class CrosapiTrustedVaultClient
    : public trusted_vault::TrustedVaultClient,
      public crosapi::mojom::TrustedVaultBackendObserver {
 public:
  // `remote` must not be null and must be bound.
  explicit CrosapiTrustedVaultClient(
      mojo::Remote<crosapi::mojom::TrustedVaultBackend>* remote);
  CrosapiTrustedVaultClient(const CrosapiTrustedVaultClient& other) = delete;
  CrosapiTrustedVaultClient& operator=(const CrosapiTrustedVaultClient& other) =
      delete;
  ~CrosapiTrustedVaultClient() override;

  // trusted_vault::TrustedVaultClient implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void FetchKeys(
      const CoreAccountInfo& account_info,
      base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)> cb)
      override;
  void StoreKeys(const std::string& gaia_id,
                 const std::vector<std::vector<uint8_t>>& keys,
                 int last_key_version) override;
  void MarkLocalKeysAsStale(const CoreAccountInfo& account_info,
                            base::OnceCallback<void(bool)> cb) override;
  void GetIsRecoverabilityDegraded(const CoreAccountInfo& account_info,
                                   base::OnceCallback<void(bool)> cb) override;
  void AddTrustedRecoveryMethod(const std::string& gaia_id,
                                const std::vector<uint8_t>& public_key,
                                int method_type_hint,
                                base::OnceClosure cb) override;
  void ClearLocalDataForAccount(const CoreAccountInfo& account_info) override;

  // crosapi::mojom::TrustedVaultBackendObserver implementation.
  void OnTrustedVaultKeysChanged() override;
  void OnTrustedVaultRecoverabilityChanged() override;

 private:
  base::ObserverList<Observer> observers_;

  // Don't add new members below this. `receiver_` and `observer_` should be
  // destroyed as soon as `this` is getting destroyed so that we don't deal with
  // message handling on a partially destroyed object.
  mojo::Receiver<crosapi::mojom::TrustedVaultBackendObserver> receiver_{this};
  raw_ptr<mojo::Remote<crosapi::mojom::TrustedVaultBackend>> remote_;
};

#endif  // CHROME_BROWSER_LACROS_TRUSTED_VAULT_CROSAPI_TRUSTED_VAULT_CLIENT_H_

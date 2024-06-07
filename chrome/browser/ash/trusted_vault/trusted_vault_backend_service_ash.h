// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TRUSTED_VAULT_TRUSTED_VAULT_BACKEND_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_TRUSTED_VAULT_TRUSTED_VAULT_BACKEND_SERVICE_ASH_H_

#include <memory>

#include "chromeos/crosapi/mojom/trusted_vault.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace signin {
class IdentityManager;
}

namespace trusted_vault {
class TrustedVaultClient;
}

namespace ash {

class TrustedVaultBackendAsh;

class TrustedVaultBackendServiceAsh
    : public KeyedService,
      public crosapi::mojom::TrustedVaultBackendService {
 public:
  // `identity_manager` and `trusted_vault_client` must not be null.
  // `passkeys_trusted_vault_client_` may be null.
  TrustedVaultBackendServiceAsh(
      signin::IdentityManager* identity_manager,
      trusted_vault::TrustedVaultClient* chrome_sync_trusted_vault_client,
      trusted_vault::TrustedVaultClient* passkeys_trusted_vault_client);
  TrustedVaultBackendServiceAsh(const TrustedVaultBackendServiceAsh&) = delete;
  TrustedVaultBackendServiceAsh& operator=(
      const TrustedVaultBackendServiceAsh&) = delete;
  ~TrustedVaultBackendServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::TrustedVaultBackendService>
          receiver);

  // KeyedService implementation.
  void Shutdown() override;

  // crosapi::mojom::TrustedVaultBackendService implementation.
  void GetTrustedVaultBackend(
      crosapi::mojom::SecurityDomainId security_domain,
      mojo::PendingReceiver<crosapi::mojom::TrustedVaultBackend> backend)
      override;

  TrustedVaultBackendAsh* chrome_sync_trusted_vault_backend() const;

 private:
  std::unique_ptr<TrustedVaultBackendAsh> chrome_sync_backend_;
  std::unique_ptr<TrustedVaultBackendAsh> passkeys_backend_;

  // Don't add new members below this. `receivers_` should be destroyed as soon
  // as `this` (or prior that) is getting destroyed so that we don't deal with
  // message handling on a partially destroyed object.
  mojo::ReceiverSet<crosapi::mojom::TrustedVaultBackendService> receivers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_TRUSTED_VAULT_TRUSTED_VAULT_BACKEND_SERVICE_ASH_H_

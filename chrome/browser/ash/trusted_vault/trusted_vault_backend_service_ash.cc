// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/trusted_vault/trusted_vault_backend_service_ash.h"

#include <utility>

#include "chrome/browser/ash/trusted_vault/trusted_vault_backend_ash.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "device/fido/features.h"

namespace ash {

TrustedVaultBackendServiceAsh::TrustedVaultBackendServiceAsh(
    signin::IdentityManager* identity_manager,
    trusted_vault::TrustedVaultClient* chrome_sync_trusted_vault_client,
    trusted_vault::TrustedVaultClient* passkeys_trusted_vault_client) {
  CHECK(identity_manager);
  CHECK(chrome_sync_trusted_vault_client);
  if (base::FeatureList::IsEnabled(
          trusted_vault::kChromeOSTrustedVaultClientShared)) {
    chrome_sync_backend_ = std::make_unique<TrustedVaultBackendAsh>(
        identity_manager, chrome_sync_trusted_vault_client);
  }
  if (passkeys_trusted_vault_client) {
    CHECK(base::FeatureList::IsEnabled(device::kChromeOsPasskeys));
    passkeys_backend_ = std::make_unique<TrustedVaultBackendAsh>(
        identity_manager, passkeys_trusted_vault_client);
  }
}

TrustedVaultBackendServiceAsh::~TrustedVaultBackendServiceAsh() = default;

void TrustedVaultBackendServiceAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::TrustedVaultBackendService>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

void TrustedVaultBackendServiceAsh::Shutdown() {
  chrome_sync_backend_.reset();
  passkeys_backend_.reset();
  receivers_.Clear();
}

void TrustedVaultBackendServiceAsh::GetTrustedVaultBackend(
    crosapi::mojom::SecurityDomainId security_domain,
    mojo::PendingReceiver<crosapi::mojom::TrustedVaultBackend>
        backend_receiver) {
  switch (security_domain) {
    case crosapi::mojom::SecurityDomainId::kUnknown:
      break;
    case crosapi::mojom::SecurityDomainId::kChromeSync:
      if (chrome_sync_backend_) {
        chrome_sync_backend_->BindReceiver(std::move(backend_receiver));
      }
      break;
    case crosapi::mojom::SecurityDomainId::kPasskeys:
      if (passkeys_backend_) {
        passkeys_backend_->BindReceiver(std::move(backend_receiver));
      }
      break;
  }
}

TrustedVaultBackendAsh*
TrustedVaultBackendServiceAsh::chrome_sync_trusted_vault_backend() const {
  CHECK(base::FeatureList::IsEnabled(
      trusted_vault::kChromeOSTrustedVaultClientShared));
  return chrome_sync_backend_.get();
}

}  // namespace ash

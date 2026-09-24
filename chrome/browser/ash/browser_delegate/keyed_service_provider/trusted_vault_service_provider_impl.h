// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_TRUSTED_VAULT_SERVICE_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_TRUSTED_VAULT_SERVICE_PROVIDER_IMPL_H_

#include "chromeos/ash/components/trusted_vault/trusted_vault_service_provider.h"

namespace ash {

class TrustedVaultServiceProviderImpl : public TrustedVaultServiceProvider {
 public:
  TrustedVaultServiceProviderImpl();
  TrustedVaultServiceProviderImpl(const TrustedVaultServiceProviderImpl&) =
      delete;
  TrustedVaultServiceProviderImpl& operator=(
      const TrustedVaultServiceProviderImpl&) = delete;
  ~TrustedVaultServiceProviderImpl() override;

  // TrustedVaultServiceProvider:
  trusted_vault::TrustedVaultService* Find(
      const AccountId& account_id) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_TRUSTED_VAULT_SERVICE_PROVIDER_IMPL_H_

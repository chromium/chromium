// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_IDENTITY_MANAGER_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_IDENTITY_MANAGER_PROVIDER_IMPL_H_

#include "chromeos/ash/components/signin/identity_manager_provider.h"

namespace ash {

class IdentityManagerProviderImpl : public IdentityManagerProvider {
 public:
  IdentityManagerProviderImpl();
  IdentityManagerProviderImpl(const IdentityManagerProviderImpl&) = delete;
  IdentityManagerProviderImpl& operator=(const IdentityManagerProviderImpl&) =
      delete;
  ~IdentityManagerProviderImpl() override;

  // IdentityManagerProvider:
  signin::IdentityManager* Find(const AccountId& account_id) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_IDENTITY_MANAGER_PROVIDER_IMPL_H_

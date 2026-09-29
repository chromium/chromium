// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_PASSWORD_REUSE_MANAGER_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_PASSWORD_REUSE_MANAGER_PROVIDER_IMPL_H_

#include "chromeos/ash/components/password_manager/password_reuse_manager_provider.h"

namespace ash {

class PasswordReuseManagerProviderImpl : public PasswordReuseManagerProvider {
 public:
  PasswordReuseManagerProviderImpl();
  PasswordReuseManagerProviderImpl(const PasswordReuseManagerProviderImpl&) =
      delete;
  PasswordReuseManagerProviderImpl& operator=(
      const PasswordReuseManagerProviderImpl&) = delete;
  ~PasswordReuseManagerProviderImpl() override;

  // PasswordReuseManagerProvider:
  password_manager::PasswordReuseManager* Find(
      const AccountId& account_id) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_PASSWORD_REUSE_MANAGER_PROVIDER_IMPL_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_kerberos_credentials_updater.h"

#include "base/check.h"

namespace ash::smb_client {

SmbKerberosCredentialsUpdater::SmbKerberosCredentialsUpdater(
    KerberosCredentialsManager* credentials_manager,
    ActiveAccountChangedCallback active_account_changed_callback)
    : credentials_manager_(credentials_manager),
      active_account_name_(credentials_manager->GetActiveAccount()),
      active_account_changed_callback_(
          std::move(active_account_changed_callback)) {
  DCHECK(credentials_manager_);
  credentials_manager_->AddObserver(this);
}

SmbKerberosCredentialsUpdater::~SmbKerberosCredentialsUpdater() {
  DCHECK(credentials_manager_);
  credentials_manager_->RemoveObserver(this);
}

bool SmbKerberosCredentialsUpdater::IsKerberosEnabled() const {
  DCHECK(credentials_manager_);
  return credentials_manager_->IsKerberosEnabled();
}

void SmbKerberosCredentialsUpdater::OnKerberosEnabledStateChanged() {
  DCHECK(credentials_manager_);

  // If Kerberos got disabled by policy, set `active_account_name_` to empty
  // string, which means no account is available.
  UpdateActiveAccount(credentials_manager_->IsKerberosEnabled()
                          ? credentials_manager_->GetActiveAccount()
                          : "");
}

void SmbKerberosCredentialsUpdater::OnAccountsChanged() {
  DCHECK(credentials_manager_);
  UpdateActiveAccount(credentials_manager_->GetActiveAccount());
}

void SmbKerberosCredentialsUpdater::UpdateActiveAccount(
    const std::string& account_name) {
  if (active_account_name_ != account_name) {
    active_account_name_ = account_name;
    active_account_changed_callback_.Run(account_name);
  }
}

}  // namespace ash::smb_client

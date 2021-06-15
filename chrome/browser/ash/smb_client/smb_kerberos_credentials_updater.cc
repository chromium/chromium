// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_kerberos_credentials_updater.h"

namespace ash {
namespace smb_client {

SmbKerberosCredentialsUpdater::SmbKerberosCredentialsUpdater(
    KerberosCredentialsManager* credentials_manager,
    ActiveAccountChangedCallback active_account_changed_callback)
    : credentials_manager_(credentials_manager),
      active_account_name_(credentials_manager->GetActiveAccount()),
      active_account_changed_callback_(
          std::move(active_account_changed_callback)) {
  credentials_manager_->AddObserver(this);
}

SmbKerberosCredentialsUpdater::~SmbKerberosCredentialsUpdater() {
  credentials_manager_->RemoveObserver(this);
}

void SmbKerberosCredentialsUpdater::OnAccountsChanged() {
  const std::string account_name = credentials_manager_->GetActiveAccount();
  if (active_account_name_ != account_name) {
    active_account_name_ = account_name;
    active_account_changed_callback_.Run(account_name);
  }
}

bool SmbKerberosCredentialsUpdater::IsKerberosEnabled() const {
  return credentials_manager_->IsKerberosEnabled();
}

}  // namespace smb_client
}  // namespace ash

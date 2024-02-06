// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_SMB_KERBEROS_CREDENTIALS_UPDATER_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_SMB_KERBEROS_CREDENTIALS_UPDATER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/kerberos/kerberos_credentials_manager.h"

namespace ash::smb_client {

// Updates Kerberos credentials in SmbService after receiving a
// `OnKerberosEnabledStateChanged` or `OnAccountsChanged` notification from
// `KerberosCredentialsManager`.
class SmbKerberosCredentialsUpdater
    : public KerberosCredentialsManager::Observer {
 public:
  using ActiveAccountChangedCallback =
      base::RepeatingCallback<void(const std::string& account_identifier)>;

  SmbKerberosCredentialsUpdater(
      KerberosCredentialsManager* credentials_manager,
      ActiveAccountChangedCallback active_account_changed_callback);
  ~SmbKerberosCredentialsUpdater() override;

  // Disallow copy and assignment.
  SmbKerberosCredentialsUpdater(const SmbKerberosCredentialsUpdater&) = delete;
  SmbKerberosCredentialsUpdater& operator=(
      const SmbKerberosCredentialsUpdater&) = delete;

  // Checks if Kerberos is enabled by asking KerberosCredentialsManager.
  bool IsKerberosEnabled() const;

  const std::string& active_account_name() const {
    return active_account_name_;
  }

 private:
  // Updates `active_account_name_`, if the given `account_name` has a different
  // value. In that case, calls `active_account_changed_callback_` with the new
  // value.
  void UpdateActiveAccount(const std::string& account_name);

  // KerberosCredentialsManager::Observer:
  void OnKerberosEnabledStateChanged() override;
  void OnAccountsChanged() override;

  // Not owned.
  raw_ptr<KerberosCredentialsManager> credentials_manager_;
  std::string active_account_name_;
  const ActiveAccountChangedCallback active_account_changed_callback_;
};

}  // namespace ash::smb_client

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_SMB_KERBEROS_CREDENTIALS_UPDATER_H_

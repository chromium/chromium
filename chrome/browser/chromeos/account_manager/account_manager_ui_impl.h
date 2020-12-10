// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_UI_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_UI_IMPL_H_

#include "chromeos/components/account_manager/account_manager_ui.h"

namespace chromeos {

class AccountManagerUIImpl : public AccountManagerUI {
 public:
  AccountManagerUIImpl();
  AccountManagerUIImpl(const AccountManagerUIImpl&) = delete;
  AccountManagerUIImpl& operator=(const AccountManagerUIImpl&) = delete;
  ~AccountManagerUIImpl() override;

 private:
  // AccountManagerUI overrides:
  void ShowAddAccountDialog(base::OnceClosure close_dialog_closure) override;
  void ShowReauthAccountDialog(const std::string& email) override;
  bool IsDialogShown() override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_UI_IMPL_H_

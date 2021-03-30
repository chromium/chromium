// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_UI_IMPL_H_
#define CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_UI_IMPL_H_

#include "ash/components/account_manager/account_manager_ui.h"
#include "base/callback_forward.h"

namespace ash {

class AccountManagerUIImpl : public AccountManagerUI {
 public:
  AccountManagerUIImpl();
  AccountManagerUIImpl(const AccountManagerUIImpl&) = delete;
  AccountManagerUIImpl& operator=(const AccountManagerUIImpl&) = delete;
  ~AccountManagerUIImpl() override;

 private:
  // AccountManagerUI overrides:
  void ShowAddAccountDialog(base::OnceClosure close_dialog_closure) override;
  void ShowReauthAccountDialog(const std::string& email,
                               base::OnceClosure close_dialog_closure) override;
  bool IsDialogShown() override;
  void ShowManageAccountsSettings() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_UI_IMPL_H_

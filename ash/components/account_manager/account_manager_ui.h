// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_UI_H_
#define ASH_COMPONENTS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_UI_H_

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/component_export.h"

namespace chromeos {

// This interface is used by `AccountManagerFacadeImpl` to show system UI
// (system dialogs, OS Settings etc.)
class COMPONENT_EXPORT(ACCOUNT_MANAGER) AccountManagerUI {
 public:
  AccountManagerUI();
  AccountManagerUI(const AccountManagerUI&) = delete;
  AccountManagerUI& operator=(const AccountManagerUI&) = delete;
  virtual ~AccountManagerUI();

  // Show system dialog for account addition.
  // `close_dialog_closure` callback will be called when dialog is closed.
  virtual void ShowAddAccountDialog(base::OnceClosure close_dialog_closure) = 0;

  // Show system dialog for account reauthentication.
  // `email` is the email of account that will be reauthenticated.
  // `close_dialog_closure` callback will be called when dialog is closed.
  virtual void ShowReauthAccountDialog(
      const std::string& email,
      base::OnceClosure close_dialog_closure) = 0;

  virtual bool IsDialogShown() = 0;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after moved to ash/
namespace ash {
using ::chromeos::AccountManagerUI;
}

#endif  // ASH_COMPONENTS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_UI_H_

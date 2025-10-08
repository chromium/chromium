// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/account_manager/account_manager_ui_impl.h"

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/check_deref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/signin/ash/inline_login_dialog.h"
#include "chromeos/ash/experiences/settings_ui/settings_app_manager.h"
#include "components/account_manager_core/account_addition_options.h"
#include "components/user_manager/user_manager.h"

namespace ash {

AccountManagerUIImpl::AccountManagerUIImpl() = default;
AccountManagerUIImpl::~AccountManagerUIImpl() = default;

void AccountManagerUIImpl::ShowAddAccountDialog(
    const account_manager::AccountAdditionOptions& options,
    base::OnceClosure close_dialog_closure) {
  InlineLoginDialog::Show(options, std::move(close_dialog_closure));
}

void AccountManagerUIImpl::ShowReauthAccountDialog(
    const std::string& email,
    base::OnceClosure close_dialog_closure) {
  InlineLoginDialog::Show(email, std::move(close_dialog_closure));
}

bool AccountManagerUIImpl::IsDialogShown() {
  return InlineLoginDialog::IsShown();
}

void AccountManagerUIImpl::ShowManageAccountsSettings() {
  ash::SettingsAppManager::Get()->Open(
      CHECK_DEREF(user_manager::UserManager::Get()->GetActiveUser()),
      {.sub_page = chromeos::settings::mojom::kPeopleSectionPath});
}

}  // namespace ash

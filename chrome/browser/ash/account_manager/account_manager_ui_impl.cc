// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/account_manager/account_manager_ui_impl.h"
#include "chrome/browser/ui/webui/signin/inline_login_dialog_chromeos.h"

namespace ash {

using ::chromeos::InlineLoginDialogChromeOS;

AccountManagerUIImpl::AccountManagerUIImpl() = default;
AccountManagerUIImpl::~AccountManagerUIImpl() = default;

void AccountManagerUIImpl::ShowAddAccountDialog(
    base::OnceClosure close_dialog_closure) {
  InlineLoginDialogChromeOS::Show(std::move(close_dialog_closure));
}

void AccountManagerUIImpl::ShowReauthAccountDialog(
    const std::string& email,
    base::OnceClosure close_dialog_closure) {
  InlineLoginDialogChromeOS::Show(email, std::move(close_dialog_closure));
}

bool AccountManagerUIImpl::IsDialogShown() {
  return InlineLoginDialogChromeOS::IsShown();
}

}  // namespace ash

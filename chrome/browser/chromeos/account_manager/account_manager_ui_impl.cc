// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/account_manager/account_manager_ui_impl.h"
#include "chrome/browser/ui/webui/signin/inline_login_dialog_chromeos.h"

namespace chromeos {

AccountManagerUIImpl::AccountManagerUIImpl() = default;
AccountManagerUIImpl::~AccountManagerUIImpl() = default;

void AccountManagerUIImpl::ShowAddAccountDialog(
    base::OnceClosure close_dialog_closure) {
  InlineLoginDialogChromeOS::Show(std::move(close_dialog_closure));
}

void AccountManagerUIImpl::ShowReauthAccountDialog(const std::string& email) {
  InlineLoginDialogChromeOS::Show(email,
                                  /*close_dialog_closure=*/base::DoNothing());
}

bool AccountManagerUIImpl::IsDialogShown() {
  return InlineLoginDialogChromeOS::IsShown();
}

}  // namespace chromeos

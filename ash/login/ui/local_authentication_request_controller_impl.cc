// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/local_authentication_request_controller_impl.h"

#include <string>
#include <utility>

#include "ash/login/ui/local_authentication_request_view.h"
#include "ash/login/ui/local_authentication_request_widget.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

LocalAuthenticationRequestControllerImpl::
    LocalAuthenticationRequestControllerImpl() = default;

LocalAuthenticationRequestControllerImpl::
    ~LocalAuthenticationRequestControllerImpl() = default;

void LocalAuthenticationRequestControllerImpl::OnClose() {}

bool LocalAuthenticationRequestControllerImpl::ShowWidget(
    LocalAuthenticationCallback local_authentication_callback,
    std::unique_ptr<UserContext> user_context) {
  if (LocalAuthenticationRequestWidget::Get()) {
    LOG(ERROR) << "LocalAuthenticationRequestWidget is already shown.";
    return false;
  }

  const auto& auth_factors = user_context->GetAuthFactorsData();
  const cryptohome::AuthFactor* local_password_factor =
      auth_factors.FindLocalPasswordFactor();
  if (local_password_factor == nullptr) {
    LOG(ERROR) << "The local password authentication factor is not available, "
                  "skip to show the local authentication dialog.";
    // TODO(b/334215182): It seems sometimes this dialog appears even when the
    // local password is not available.
    base::debug::DumpWithoutCrashing();
    return false;
  }

  const AccountId& account_id = user_context->GetAccountId();

  const std::string& user_email = account_id.GetUserEmail();

  const std::u16string desc = l10n_util::GetStringFUTF16(
      IDS_ASH_LOGIN_LOCAL_AUTHENTICATION_REQUEST_DESCRIPTION,
      base::UTF8ToUTF16(user_email));

  LocalAuthenticationRequestWidget::Show(
      std::move(local_authentication_callback),
      l10n_util::GetStringUTF16(
          IDS_ASH_LOGIN_LOCAL_AUTHENTICATION_REQUEST_TITLE),
      desc, weak_factory_.GetWeakPtr(), std::move(user_context));
  return true;
}

}  // namespace ash

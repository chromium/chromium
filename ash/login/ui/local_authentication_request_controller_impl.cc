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
    OnLocalAuthenticationCompleted on_local_authentication_completed,
    std::unique_ptr<UserContext> user_context) {
  if (LocalAuthenticationRequestWidget::Get()) {
    LOG(ERROR) << "LocalAuthenticationRequestWidget is already shown.";
    return false;
  }

  const AccountId& account_id = user_context->GetAccountId();

  const std::string& user_email = account_id.GetUserEmail();

  const std::u16string desc = l10n_util::GetStringFUTF16(
      IDS_ASH_LOGIN_LOCAL_AUTHENTICATION_REQUEST_DESCRIPTION,
      base::UTF8ToUTF16(user_email));

  LocalAuthenticationRequestWidget::Show(
      std::move(on_local_authentication_completed),
      l10n_util::GetStringUTF16(
          IDS_ASH_LOGIN_LOCAL_AUTHENTICATION_REQUEST_TITLE),
      desc, this, std::move(user_context));
  return true;
}

}  // namespace ash

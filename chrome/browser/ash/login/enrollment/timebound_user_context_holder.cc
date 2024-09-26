// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/timebound_user_context_holder.h"

#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/screens/account_selection_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/account_selection_screen_handler.h"

namespace ash {

TimeboundUserContextHolder::TimeboundUserContextHolder(
    std::unique_ptr<UserContext> user_context)
    : user_context_{std::move(user_context)},
      token_revoker_{std::make_unique<OAuth2TokenRevoker>()} {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TimeboundUserContextHolder::OnTimeout,
                     weak_ptr_factory_.GetWeakPtr()),
      kCredentialsVlidityPeriod);
  session_observation_.Observe(session_manager::SessionManager::Get());
}

TimeboundUserContextHolder::~TimeboundUserContextHolder() {
  if (!user_context_) {
    return;
  }
  ClearUserContext();
}

void TimeboundUserContextHolder::OnTimeout() {
  if (!user_context_) {
    return;
  }
  ClearUserContext();
  if (!LoginDisplayHost::default_host()) {
    return;
  }
  WizardContext* context = LoginDisplayHost::default_host()->GetWizardContext();
  if (context) {
    context->timebound_user_context_holder.reset();
  }
  WizardController* controller =
      LoginDisplayHost::default_host()->GetWizardController();
  if (!controller) {
    return;
  }
  AccountSelectionScreen* screen =
      controller->GetScreen<AccountSelectionScreen>();
  if (!screen) {
    return;
  }
  screen->OnCredentialsExpiredCallback();
}

void TimeboundUserContextHolder::ClearUserContext() {
  session_observation_.Reset();
  if (!user_context_) {
    return;
  }
  if (!user_context_->GetAccessToken().empty()) {
    token_revoker_->Start(user_context_->GetAccessToken());
  }
  if (!user_context_->GetRefreshToken().empty()) {
    token_revoker_->Start(user_context_->GetRefreshToken());
  }
  user_context_.reset();
}

void TimeboundUserContextHolder::OnSessionStateChanged() {
  if (!user_context_) {
    return;
  }
  session_manager::SessionState state =
      session_manager::SessionManager::Get()->session_state();
  if (state == session_manager::SessionState::LOGGED_IN_NOT_ACTIVE ||
      state == session_manager::SessionState::ACTIVE) {
    ClearUserContext();
  }
}

}  // namespace ash

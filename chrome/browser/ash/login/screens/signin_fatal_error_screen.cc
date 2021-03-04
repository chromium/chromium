// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/signin_fatal_error_screen.h"

#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_fatal_error_screen_handler.h"

namespace {
constexpr char kUserActionScreenDismissed[] = "screen-dismissed";
}  // namespace

namespace chromeos {

SignInFatalErrorScreen::SignInFatalErrorScreen(
    SignInFatalErrorView* view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(SignInFatalErrorView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  view_->Bind(this);
}

SignInFatalErrorScreen::~SignInFatalErrorScreen() {
  if (view_)
    view_->Unbind();
}

void SignInFatalErrorScreen::OnViewDestroyed(SignInFatalErrorView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void SignInFatalErrorScreen::SetErrorState(Error error,
                                           const base::Value* params) {
  error_state_ = error;
  extra_error_info_ = params ? base::make_optional<base::Value>(params->Clone())
                             : base::nullopt;
}

void SignInFatalErrorScreen::ShowImpl() {
  if (!view_)
    return;

  view_->Show(error_state_, base::OptionalOrNullptr(extra_error_info_));
}

void SignInFatalErrorScreen::HideImpl() {}

void SignInFatalErrorScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionScreenDismissed) {
    exit_callback_.Run();
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

}  // namespace chromeos

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/signin_fatal_error_screen.h"

#include "base/values.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/signin_fatal_error_screen_handler.h"

namespace ash {
namespace {

constexpr char kUserActionScreenDismissed[] = "screen-dismissed";
constexpr char kUserActionLearnMore[] = "learn-more";

}  // namespace

SignInFatalErrorScreen::SignInFatalErrorScreen(
    base::WeakPtr<SignInFatalErrorView> view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(SignInFatalErrorView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  DCHECK(view_);
}

SignInFatalErrorScreen::~SignInFatalErrorScreen() = default;

void SignInFatalErrorScreen::SetErrorState(Error error,
                                           base::Value::Dict params) {
  error_state_ = error;
  extra_error_info_ = std::move(params);
}

void SignInFatalErrorScreen::SetCustomError(const std::string& error_text,
                                            const std::string& keyboard_hint,
                                            const std::string& details,
                                            const std::string& help_link_text) {
  error_state_ = Error::kCustom;
  extra_error_info_ = base::Value::Dict();
  DCHECK(!error_text.empty());
  extra_error_info_.Set("errorText", error_text);
  if (!keyboard_hint.empty()) {
    extra_error_info_.Set("keyboardHint", keyboard_hint);
  }
  if (!details.empty()) {
    extra_error_info_.Set("details", details);
  }
  if (!help_link_text.empty()) {
    extra_error_info_.Set("helpLinkText", help_link_text);
  }
}

void SignInFatalErrorScreen::ShowImpl() {
  if (!view_)
    return;

  view_->Show(error_state_, extra_error_info_);
}

void SignInFatalErrorScreen::HideImpl() {}

void SignInFatalErrorScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionScreenDismissed) {
    exit_callback_.Run();
  } else if (action_id == kUserActionLearnMore) {
    if (!help_app_.get()) {
      help_app_ = new HelpAppLauncher(
          LoginDisplayHost::default_host()->GetNativeWindow());
    }
    help_app_->ShowHelpTopic(HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash

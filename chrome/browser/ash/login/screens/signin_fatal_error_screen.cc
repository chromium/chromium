// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/signin_fatal_error_screen.h"

#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_fatal_error_screen_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace {

constexpr char kUserActionScreenDismissed[] = "screen-dismissed";
constexpr char kUserActionLearnMore[] = "learn-more";

}  // namespace

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
  extra_error_info_ = params ? absl::make_optional<base::Value>(params->Clone())
                             : absl::nullopt;
}

void SignInFatalErrorScreen::SetCustomError(const std::string& error_text,
                                            const std::string& keyboard_hint,
                                            const std::string& details,
                                            const std::string& help_link_text) {
  error_state_ = Error::CUSTOM;
  extra_error_info_ =
      absl::make_optional<base::Value>(base::Value::Type::DICTIONARY);
  DCHECK(!error_text.empty());
  extra_error_info_->SetStringKey("errorText", error_text);
  if (!keyboard_hint.empty()) {
    extra_error_info_->SetStringKey("keyboardHint", keyboard_hint);
  }
  if (!details.empty()) {
    extra_error_info_->SetStringKey("details", details);
  }
  if (!help_link_text.empty()) {
    extra_error_info_->SetStringKey("helpLinkText", help_link_text);
  }
}

void SignInFatalErrorScreen::ShowImpl() {
  if (!view_)
    return;

  view_->Show(error_state_, base::OptionalOrNullptr(extra_error_info_));
}

void SignInFatalErrorScreen::HideImpl() {}

void SignInFatalErrorScreen::OnUserActionDeprecated(
    const std::string& action_id) {
  if (action_id == kUserActionScreenDismissed) {
    exit_callback_.Run();
  } else if (action_id == kUserActionLearnMore) {
    if (!help_app_.get()) {
      help_app_ = new HelpAppLauncher(
          LoginDisplayHost::default_host()->GetNativeWindow());
    }
    help_app_->ShowHelpTopic(HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
  } else {
    BaseScreen::OnUserActionDeprecated(action_id);
  }
}

}  // namespace ash

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/local_password_setup_screen.h"

#include "base/values.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ui/webui/ash/login/local_password_setup_handler.h"

namespace ash {

// static
std::string LocalPasswordSetupScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kDone:
      return "Done";
    case Result::kBack:
      return "Back";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
}

LocalPasswordSetupScreen::LocalPasswordSetupScreen(
    base::WeakPtr<LocalPasswordSetupView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(LocalPasswordSetupView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

LocalPasswordSetupScreen::~LocalPasswordSetupScreen() = default;

void LocalPasswordSetupScreen::ShowImpl() {
  if (!view_) {
    return;
  }
  view_->Show();
}

void LocalPasswordSetupScreen::HideImpl() {}

void LocalPasswordSetupScreen::OnUserAction(const base::Value::List& args) {
  BaseScreen::OnUserAction(args);
}

}  // namespace ash

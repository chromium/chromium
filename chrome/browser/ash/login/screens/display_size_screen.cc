// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/display_size_screen.h"

#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/display_size_screen_handler.h"

namespace ash {
namespace {

constexpr const char kUserActionNext[] = "next";

}  // namespace

// static
std::string DisplaySizeScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kNext:
      return "Next";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
}

DisplaySizeScreen::DisplaySizeScreen(base::WeakPtr<DisplaySizeScreenView> view,
                                     const ScreenExitCallback& exit_callback)
    : BaseScreen(DisplaySizeScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

DisplaySizeScreen::~DisplaySizeScreen() = default;

bool DisplaySizeScreen::MaybeSkip(WizardContext& context) {
  if (context.skip_post_login_screens_for_tests) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  return false;
}

void DisplaySizeScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  view_->Show();
}

void DisplaySizeScreen::HideImpl() {}

void DisplaySizeScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionNext) {
    // TODO(b/275556512): Include the screen In CHOOBE flow.
    NOTIMPLEMENTED();
    return;
  }
  BaseScreen::OnUserAction(args);
}

}  // namespace ash

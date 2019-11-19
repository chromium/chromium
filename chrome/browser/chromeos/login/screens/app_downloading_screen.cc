// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/app_downloading_screen.h"

#include "chrome/browser/ui/webui/chromeos/login/app_downloading_screen_handler.h"

namespace chromeos {
namespace {

// When user clicks "Continue setup", this will be sent to chrome to indicate
// that user is proceeding to the next step.
constexpr const char kUserActionButtonContinueSetup[] =
    "appDownloadingContinueSetup";

}  // namespace

AppDownloadingScreen::AppDownloadingScreen(
    AppDownloadingScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(AppDownloadingScreenView::kScreenId),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  view_->Bind(this);
}

AppDownloadingScreen::~AppDownloadingScreen() {
  view_->Bind(nullptr);
}

void AppDownloadingScreen::Show() {
  // Show the screen.
  view_->Show();
}

void AppDownloadingScreen::Hide() {
  view_->Hide();
}

void AppDownloadingScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionButtonContinueSetup) {
    exit_callback_.Run();
    return;
  }
  BaseScreen::OnUserAction(action_id);
}

}  // namespace chromeos

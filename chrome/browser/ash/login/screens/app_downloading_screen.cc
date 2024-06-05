// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/app_downloading_screen.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/app_downloading_screen_handler.h"

namespace ash {

AppDownloadingScreen::AppDownloadingScreen(
    base::WeakPtr<AppDownloadingScreenView> view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(AppDownloadingScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      OobeMojoBinder(this),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

AppDownloadingScreen::~AppDownloadingScreen() = default;

void AppDownloadingScreen::ShowImpl() {
  // Show the screen.
  view_->Show();
}

void AppDownloadingScreen::HideImpl() {}

void AppDownloadingScreen::OnContinueClicked() {
  if (is_hidden()) {
    return;
  }
  exit_callback_.Run();
}

}  // namespace ash

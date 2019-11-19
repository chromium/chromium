// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/supervision_transition_screen.h"

#include "chrome/browser/ui/webui/chromeos/login/supervision_transition_screen_handler.h"

namespace chromeos {

SupervisionTransitionScreen::SupervisionTransitionScreen(
    SupervisionTransitionScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(SupervisionTransitionScreenView::kScreenId),
      view_(view),
      exit_callback_(exit_callback) {
  if (view_)
    view_->Bind(this);
}

SupervisionTransitionScreen::~SupervisionTransitionScreen() {
  if (view_)
    view_->Unbind();
}

void SupervisionTransitionScreen::Show() {
  if (view_)
    view_->Show();
}

void SupervisionTransitionScreen::Hide() {
  if (view_)
    view_->Hide();
}

void SupervisionTransitionScreen::OnViewDestroyed(
    SupervisionTransitionScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void SupervisionTransitionScreen::OnSupervisionTransitionFinished() {
  exit_callback_.Run();
}

}  // namespace chromeos

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/management_transition_screen.h"

#include "chrome/browser/ui/webui/chromeos/login/management_transition_screen_handler.h"

namespace ash {

ManagementTransitionScreen::ManagementTransitionScreen(
    ManagementTransitionScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(ManagementTransitionScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  if (view_)
    view_->Bind(this);
}

ManagementTransitionScreen::~ManagementTransitionScreen() {
  if (view_)
    view_->Unbind();
}

void ManagementTransitionScreen::ShowImpl() {
  if (view_)
    view_->Show();
}

void ManagementTransitionScreen::HideImpl() {
  if (view_)
    view_->Hide();
}

void ManagementTransitionScreen::OnViewDestroyed(
    ManagementTransitionScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void ManagementTransitionScreen::OnManagementTransitionFinished() {
  exit_callback_.Run();
}

}  // namespace ash

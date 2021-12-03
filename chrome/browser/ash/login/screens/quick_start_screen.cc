// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/quick_start_screen.h"

#include "chrome/browser/ui/webui/chromeos/login/quick_start_screen_handler.h"

namespace ash {

// static
std::string QuickStartScreen::GetResultString(Result result) {
  switch (result) {
    case Result::CANCEL:
      return "Cancel";
  }
}

QuickStartScreen::QuickStartScreen(QuickStartView* view,
                                   const ScreenExitCallback& exit_callback)
    : BaseScreen(QuickStartView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  if (view_)
    view_->Bind(this);
}

QuickStartScreen::~QuickStartScreen() {
  if (view_)
    view_->Unbind();
}

void QuickStartScreen::OnViewDestroyed(TView* view) {
  if (view_ == view)
    view_ = nullptr;
}

bool QuickStartScreen::MaybeSkip(WizardContext* context) {
  return false;
}

void QuickStartScreen::ShowImpl() {
  if (view_) {
    view_->Show();
  }
}

void QuickStartScreen::HideImpl() {}

void QuickStartScreen::OnUserAction(const std::string& action_id) {
  BaseScreen::OnUserAction(action_id);
}

}  // namespace ash

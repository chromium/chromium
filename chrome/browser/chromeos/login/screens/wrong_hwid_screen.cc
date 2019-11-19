// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/wrong_hwid_screen.h"

#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/wrong_hwid_screen_handler.h"

namespace chromeos {

WrongHWIDScreen::WrongHWIDScreen(WrongHWIDScreenView* view,
                                 const base::RepeatingClosure& exit_callback)
    : BaseScreen(WrongHWIDScreenView::kScreenId),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  if (view_)
    view_->SetDelegate(this);
}

WrongHWIDScreen::~WrongHWIDScreen() {
  if (view_)
    view_->SetDelegate(nullptr);
}

void WrongHWIDScreen::OnExit() {
  exit_callback_.Run();
}

void WrongHWIDScreen::OnViewDestroyed(WrongHWIDScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void WrongHWIDScreen::Show() {
  if (view_)
    view_->Show();
}

void WrongHWIDScreen::Hide() {
  if (view_)
    view_->Hide();
}

}  // namespace chromeos

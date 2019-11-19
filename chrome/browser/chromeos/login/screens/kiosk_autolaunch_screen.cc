// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/kiosk_autolaunch_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/kiosk_autolaunch_screen_handler.h"

namespace chromeos {

KioskAutolaunchScreen::KioskAutolaunchScreen(
    KioskAutolaunchScreenView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(KioskAutolaunchScreenView::kScreenId),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  if (view_)
    view_->SetDelegate(this);
}

KioskAutolaunchScreen::~KioskAutolaunchScreen() {
  if (view_)
    view_->SetDelegate(NULL);
}

void KioskAutolaunchScreen::OnExit(bool confirmed) {
  exit_callback_.Run(confirmed ? Result::COMPLETED : Result::CANCELED);
}

void KioskAutolaunchScreen::OnViewDestroyed(KioskAutolaunchScreenView* view) {
  if (view_ == view)
    view_ = NULL;
}

void KioskAutolaunchScreen::Show() {
  if (view_)
    view_->Show();
}

void KioskAutolaunchScreen::Hide() {}

}  // namespace chromeos

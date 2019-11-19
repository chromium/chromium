// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/kiosk_enable_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/kiosk_enable_screen_handler.h"

namespace chromeos {

KioskEnableScreen::KioskEnableScreen(
    KioskEnableScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(KioskEnableScreenView::kScreenId),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  if (view_)
    view_->SetDelegate(this);
}

KioskEnableScreen::~KioskEnableScreen() {
  if (view_)
    view_->SetDelegate(NULL);
}

void KioskEnableScreen::OnExit() {
  exit_callback_.Run();
}

void KioskEnableScreen::OnViewDestroyed(KioskEnableScreenView* view) {
  if (view_ == view)
    view_ = NULL;
}

void KioskEnableScreen::Show() {
  if (view_)
    view_->Show();
}

void KioskEnableScreen::Hide() {}

}  // namespace chromeos

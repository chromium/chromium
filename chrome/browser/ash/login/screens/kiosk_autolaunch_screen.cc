// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/kiosk_autolaunch_screen.h"

#include "base/check.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/ui/webui/chromeos/login/kiosk_autolaunch_screen_handler.h"

namespace chromeos {

// static
std::string KioskAutolaunchScreen::GetResultString(Result result) {
  switch (result) {
    case Result::COMPLETED:
      return "Completed";
    case Result::CANCELED:
      return "Canceled";
  }
}

KioskAutolaunchScreen::KioskAutolaunchScreen(
    KioskAutolaunchScreenView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(KioskAutolaunchScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
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
  if (is_hidden())
    return;
  exit_callback_.Run(confirmed ? Result::COMPLETED : Result::CANCELED);
}

void KioskAutolaunchScreen::OnViewDestroyed(KioskAutolaunchScreenView* view) {
  if (view_ == view)
    view_ = NULL;
}

void KioskAutolaunchScreen::ShowImpl() {
  if (view_)
    view_->Show();
}

void KioskAutolaunchScreen::HideImpl() {}

}  // namespace chromeos

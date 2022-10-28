// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/kiosk_autolaunch_screen.h"

#include "base/check.h"
#include "chrome/browser/ash/customization/customization_document.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/kiosk_autolaunch_screen_handler.h"

namespace {

constexpr char kUserActionOnCancel[] = "cancel";
constexpr char kUserActionOnConfirm[] = "confirm";

}  // namespace

namespace ash {

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
    base::WeakPtr<KioskAutolaunchScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(KioskAutolaunchScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  DCHECK(view_);
}

KioskAutolaunchScreen::~KioskAutolaunchScreen() = default;

void KioskAutolaunchScreen::OnExit(bool confirmed) {
  if (is_hidden())
    return;
  exit_callback_.Run(confirmed ? Result::COMPLETED : Result::CANCELED);
}

void KioskAutolaunchScreen::ShowImpl() {
  if (view_)
    view_->Show();
}

void KioskAutolaunchScreen::HideImpl() {}

void KioskAutolaunchScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionOnCancel) {
    if (view_) {
      view_->HandleOnCancel();
    }
    OnExit(false);
  } else if (action_id == kUserActionOnConfirm) {
    if (view_) {
      view_->HandleOnConfirm();
    }
    OnExit(true);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash

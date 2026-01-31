// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/fjord_fw_update_screen.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/fjord_fw_update_screen_handler.h"

namespace ash {

FjordFwUpdateScreen::FjordFwUpdateScreen(
    base::WeakPtr<FjordFwUpdateScreenView> view,
    base::RepeatingClosure exit_callback)
    : BaseScreen(FjordFwUpdateScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

FjordFwUpdateScreen::~FjordFwUpdateScreen() = default;

bool FjordFwUpdateScreen::ExitScreen() {
  if (is_hidden()) {
    return false;
  }

  exit_callback_.Run();
  return true;
}

void FjordFwUpdateScreen::ShowImpl() {
  view_->Show();
}

}  // namespace ash

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/fjord_station_setup_screen.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/fjord_station_setup_screen_handler.h"

namespace ash {

FjordStationSetupScreen::FjordStationSetupScreen(
    base::WeakPtr<FjordStationSetupScreenView> view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(FjordStationSetupScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      OobeMojoBinder(this),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

FjordStationSetupScreen::~FjordStationSetupScreen() = default;

void FjordStationSetupScreen::ShowImpl() {
  view_->Show();
}

void FjordStationSetupScreen::OnSetupComplete() {
  if (is_hidden()) {
    return;
  }
  exit_callback_.Run();
}

}  // namespace ash

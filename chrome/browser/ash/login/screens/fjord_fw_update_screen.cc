// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/fjord_fw_update_screen.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/fjord_fw_update_screen_handler.h"

namespace ash {

FjordFwUpdateScreen::FjordFwUpdateScreen(
    base::WeakPtr<FjordFwUpdateScreenView> view)
    : BaseScreen(FjordFwUpdateScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)) {}

FjordFwUpdateScreen::~FjordFwUpdateScreen() = default;

void FjordFwUpdateScreen::ShowImpl() {
  view_->Show();
}

}  // namespace ash

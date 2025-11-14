// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/fjord_touch_controller_screen.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/fjord_touch_controller_screen_handler.h"

namespace ash {

FjordTouchControllerScreen::FjordTouchControllerScreen(
    base::WeakPtr<FjordTouchControllerScreenView> view)
    : BaseScreen(FjordTouchControllerScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)) {}

FjordTouchControllerScreen::~FjordTouchControllerScreen() = default;

void FjordTouchControllerScreen::ShowImpl() {
  view_->Show();
}

}  // namespace ash

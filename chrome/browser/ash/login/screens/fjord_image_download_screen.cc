// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/fjord_image_download_screen.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/fjord_image_download_screen_handler.h"

namespace ash {

FjordImageDownloadScreen::FjordImageDownloadScreen(
    base::WeakPtr<FjordImageDownloadScreenView> view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(FjordImageDownloadScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      exit_callback_(exit_callback),
      view_(std::move(view)) {}

FjordImageDownloadScreen::~FjordImageDownloadScreen() = default;

void FjordImageDownloadScreen::ShowImpl() {
  view_->Show();
}

}  // namespace ash

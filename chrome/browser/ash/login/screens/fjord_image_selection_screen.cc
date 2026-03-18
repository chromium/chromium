// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/fjord_image_selection_screen.h"

#include <utility>

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/fjord_image_selection_screen_handler.h"

namespace ash {

// static
// LINT.IfChange(UsageMetrics)
std::string FjordImageSelectionScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kCuttlefish:
      return "Cuttlefish";
    case Result::kSquid:
      return "Squid";
  }
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)

FjordImageSelectionScreen::FjordImageSelectionScreen(
    base::WeakPtr<FjordImageSelectionScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(FjordImageSelectionScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      OobeMojoBinder(this),
      exit_callback_(exit_callback),
      view_(std::move(view)) {}

FjordImageSelectionScreen::~FjordImageSelectionScreen() = default;

void FjordImageSelectionScreen::ShowImpl() {
  view_->Show();
}

void FjordImageSelectionScreen::OnImageSelected(
    screens_common::mojom::FjordImageSelectionPageHandler::FjordImageType
        image_type) {
  if (is_hidden()) {
    return;
  }

  Result result;
  switch (image_type) {
    case screens_common::mojom::FjordImageSelectionPageHandler::FjordImageType::
        kCuttlefish:
      result = Result::kCuttlefish;
      break;
    case screens_common::mojom::FjordImageSelectionPageHandler::FjordImageType::
        kSquid:
      result = Result::kSquid;
      break;
  }
  exit_callback_.Run(result);
}

}  // namespace ash

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_source_view.h"

#include <memory>

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

CaptureModeSourceView::CaptureModeSourceView()
    : fullscreen_toggle_button_(
          AddChildView(std::make_unique<CaptureModeToggleButton>(
              base::BindRepeating(&CaptureModeSourceView::OnFullscreenToggle,
                                  base::Unretained(this)),
              kCaptureModeFullscreenIcon))),
      region_toggle_button_(
          AddChildView(std::make_unique<CaptureModeToggleButton>(
              base::BindRepeating(&CaptureModeSourceView::OnRegionToggle,
                                  base::Unretained(this)),
              kCaptureModeRegionIcon))),
      window_toggle_button_(
          AddChildView(std::make_unique<CaptureModeToggleButton>(
              base::BindRepeating(&CaptureModeSourceView::OnWindowToggle,
                                  base::Unretained(this)),
              kCaptureModeWindowIcon))) {
  auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      capture_mode::kBetweenChildSpacing));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  auto* controller = CaptureModeController::Get();
  OnCaptureSourceChanged(controller->source());
  OnCaptureTypeChanged(controller->type());
}

CaptureModeSourceView::~CaptureModeSourceView() = default;

void CaptureModeSourceView::OnCaptureSourceChanged(
    CaptureModeSource new_source) {
  fullscreen_toggle_button_->SetToggled(new_source ==
                                        CaptureModeSource::kFullscreen);
  region_toggle_button_->SetToggled(new_source == CaptureModeSource::kRegion);
  window_toggle_button_->SetToggled(new_source == CaptureModeSource::kWindow);
}

void CaptureModeSourceView::OnCaptureTypeChanged(CaptureModeType new_type) {
  const bool is_capturing_image = new_type == CaptureModeType::kImage;
  fullscreen_toggle_button_->SetTooltipText(l10n_util::GetStringUTF16(
      is_capturing_image ? IDS_ASH_SCREEN_CAPTURE_TOOLTIP_FULLSCREEN_SCREENSHOT
                         : IDS_ASH_SCREEN_CAPTURE_TOOLTIP_FULLSCREEN_RECORD));
  region_toggle_button_->SetTooltipText(l10n_util::GetStringUTF16(
      is_capturing_image ? IDS_ASH_SCREEN_CAPTURE_TOOLTIP_REGION_SCREENSHOT
                         : IDS_ASH_SCREEN_CAPTURE_TOOLTIP_REGION_RECORD));
  window_toggle_button_->SetTooltipText(l10n_util::GetStringUTF16(
      is_capturing_image ? IDS_ASH_SCREEN_CAPTURE_TOOLTIP_WINDOW_SCREENSHOT
                         : IDS_ASH_SCREEN_CAPTURE_TOOLTIP_WINDOW_RECORD));
}

void CaptureModeSourceView::OnFullscreenToggle() {
  RecordCaptureModeBarButtonType(CaptureModeBarButtonType::kFull);
  CaptureModeController::Get()->SetSource(CaptureModeSource::kFullscreen);
}

void CaptureModeSourceView::OnRegionToggle() {
  RecordCaptureModeBarButtonType(CaptureModeBarButtonType::kRegion);
  CaptureModeController::Get()->SetSource(CaptureModeSource::kRegion);
}

void CaptureModeSourceView::OnWindowToggle() {
  RecordCaptureModeBarButtonType(CaptureModeBarButtonType::kWindow);
  CaptureModeController::Get()->SetSource(CaptureModeSource::kWindow);
}

BEGIN_METADATA(CaptureModeSourceView, views::View)
END_METADATA

}  // namespace ash

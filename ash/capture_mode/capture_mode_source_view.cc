// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_source_view.h"

#include <memory>

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/tab_slider.h"
#include "ash/style/tab_slider_button.h"
#include "base/functional/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

CaptureModeSourceView::CaptureModeSourceView()
    : capture_source_switch_(AddChildView(std::make_unique<TabSlider>(
          /*max_tab_num=*/3,
          TabSlider::InitParams{
              /*internal_border_padding=*/0,
              /*between_child_spacing=*/capture_mode::kBetweenChildSpacing,
              /*has_background=*/false,
              /*has_selector_animation=*/false,
              /*distribute_space_evenly=*/true}))),
      fullscreen_toggle_button_(
          capture_source_switch_->AddButton<IconSliderButton>(
              base::BindRepeating(&CaptureModeSourceView::OnFullscreenToggle,
                                  base::Unretained(this)),
              &kCaptureModeFullscreenIcon,
              // Tooltip text will be set in `OnCaptureTypeChanged`.
              /*tooltip_text=*/u"")),
      region_toggle_button_(capture_source_switch_->AddButton<IconSliderButton>(
          base::BindRepeating(&CaptureModeSourceView::OnRegionToggle,
                              base::Unretained(this)),
          &kCaptureModeRegionIcon,
          // Tooltip text will be set in `OnCaptureTypeChanged`.
          /*tooltip_text=*/u"")),
      window_toggle_button_(capture_source_switch_->AddButton<IconSliderButton>(
          base::BindRepeating(&CaptureModeSourceView::OnWindowToggle,
                              base::Unretained(this)),
          &kCaptureModeWindowIcon,
          // Tooltip text will be set in `OnCaptureTypeChanged`.
          /*tooltip_text=*/u"")) {
  // Add highlight helper to each toggle button.
  for (auto* button : {
           fullscreen_toggle_button_.get(),
           region_toggle_button_.get(),
           window_toggle_button_.get(),
       }) {
    CaptureModeSessionFocusCycler::HighlightHelper::Install(button);
  }

  auto* controller = CaptureModeController::Get();
  OnCaptureSourceChanged(controller->source());
  OnCaptureTypeChanged(controller->type());

  SetLayoutManager(std::make_unique<views::FillLayout>());
}

CaptureModeSourceView::~CaptureModeSourceView() = default;

void CaptureModeSourceView::OnCaptureSourceChanged(
    CaptureModeSource new_source) {
  switch (new_source) {
    case CaptureModeSource::kFullscreen:
      fullscreen_toggle_button_->SetSelected(true);
      break;
    case CaptureModeSource::kRegion:
      region_toggle_button_->SetSelected(true);
      break;
    case CaptureModeSource::kWindow:
      window_toggle_button_->SetSelected(true);
      break;
  }
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

BEGIN_METADATA(CaptureModeSourceView)
END_METADATA

}  // namespace ash

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_source_view.h"

#include <memory>

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/bind.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

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
  OnCaptureSourceChanged(CaptureModeController::Get()->source());
}

CaptureModeSourceView::~CaptureModeSourceView() = default;

void CaptureModeSourceView::OnCaptureSourceChanged(
    CaptureModeSource new_source) {
  fullscreen_toggle_button_->SetToggled(new_source ==
                                        CaptureModeSource::kFullscreen);
  region_toggle_button_->SetToggled(new_source == CaptureModeSource::kRegion);
  window_toggle_button_->SetToggled(new_source == CaptureModeSource::kWindow);
}

void CaptureModeSourceView::OnFullscreenToggle() {
  CaptureModeController::Get()->SetSource(CaptureModeSource::kFullscreen);
}

void CaptureModeSourceView::OnRegionToggle() {
  CaptureModeController::Get()->SetSource(CaptureModeSource::kRegion);
}

void CaptureModeSourceView::OnWindowToggle() {
  CaptureModeController::Get()->SetSource(CaptureModeSource::kWindow);
}

BEGIN_METADATA(CaptureModeSourceView, views::View)
END_METADATA

}  // namespace ash

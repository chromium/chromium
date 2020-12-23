// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_type_view.h"

#include <memory>

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "base/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

namespace {

constexpr int kBackgroundCornerRadius = 18;

constexpr gfx::Insets kViewInsets{2};

constexpr int kButtonSpacing = 2;

}  // namespace

CaptureModeTypeView::CaptureModeTypeView()
    : image_toggle_button_(
          AddChildView(std::make_unique<CaptureModeToggleButton>(
              base::BindRepeating(&CaptureModeTypeView::OnImageToggle,
                                  base::Unretained(this)),
              kCaptureModeImageIcon))),
      video_toggle_button_(
          AddChildView(std::make_unique<CaptureModeToggleButton>(
              base::BindRepeating(&CaptureModeTypeView::OnVideoToggle,
                                  base::Unretained(this)),
              kCaptureModeVideoIcon))) {
  auto* color_provider = AshColorProvider::Get();
  const SkColor bg_color = color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  SetBackground(
      views::CreateRoundedRectBackground(bg_color, kBackgroundCornerRadius));
  SetBorder(views::CreateEmptyBorder(kViewInsets));
  auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(0),
      kButtonSpacing));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  auto* controller = CaptureModeController::Get();
  // We can't have more than one recording at the same time.
  video_toggle_button_->SetEnabled(!controller->is_recording_in_progress());
  OnCaptureTypeChanged(controller->type());

  image_toggle_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_TOOLTIP_SCREENSHOT));
  video_toggle_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_TOOLTIP_SCREENRECORD));
}

CaptureModeTypeView::~CaptureModeTypeView() = default;

void CaptureModeTypeView::OnCaptureTypeChanged(CaptureModeType new_type) {
  DCHECK(!CaptureModeController::Get()->is_recording_in_progress() ||
      new_type == CaptureModeType::kImage);
  image_toggle_button_->SetToggled(new_type == CaptureModeType::kImage);
  video_toggle_button_->SetToggled(new_type == CaptureModeType::kVideo);
  DCHECK_NE(image_toggle_button_->GetToggled(),
            video_toggle_button_->GetToggled());
}

void CaptureModeTypeView::OnImageToggle() {
  RecordCaptureModeBarButtonType(CaptureModeBarButtonType::kScreenCapture);
  CaptureModeController::Get()->SetType(CaptureModeType::kImage);
}

void CaptureModeTypeView::OnVideoToggle() {
  auto* controller = CaptureModeController::Get();
  DCHECK(!controller->is_recording_in_progress());
  RecordCaptureModeBarButtonType(CaptureModeBarButtonType::kScreenRecord);
  controller->SetType(CaptureModeType::kVideo);
}

BEGIN_METADATA(CaptureModeTypeView, views::View)
END_METADATA

}  // namespace ash

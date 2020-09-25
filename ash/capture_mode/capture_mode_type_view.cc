// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_type_view.h"

#include <memory>

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kBackgroundCornerRadius = 18;

constexpr gfx::Insets kViewInsets{2};

constexpr int kButtonSpacing = 2;

}  // namespace

CaptureModeTypeView::CaptureModeTypeView()
    : image_toggle_button_(AddChildView(
          std::make_unique<CaptureModeToggleButton>(this,
                                                    kCaptureModeImageIcon))),
      video_toggle_button_(AddChildView(
          std::make_unique<CaptureModeToggleButton>(this,
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
  OnCaptureTypeChanged(CaptureModeController::Get()->type());
}

CaptureModeTypeView::~CaptureModeTypeView() = default;

void CaptureModeTypeView::OnCaptureTypeChanged(CaptureModeType new_type) {
  image_toggle_button_->SetToggled(new_type == CaptureModeType::kImage);
  video_toggle_button_->SetToggled(new_type == CaptureModeType::kVideo);
  DCHECK_NE(image_toggle_button_->GetToggled(),
            video_toggle_button_->GetToggled());
}

const char* CaptureModeTypeView::GetClassName() const {
  return "CaptureModeTypeView";
}

void CaptureModeTypeView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  auto* controller = CaptureModeController::Get();
  if (sender == image_toggle_button_) {
    controller->SetType(CaptureModeType::kImage);
  } else {
    DCHECK_EQ(sender, video_toggle_button_);
    controller->SetType(CaptureModeType::kVideo);
  }
}

}  // namespace ash

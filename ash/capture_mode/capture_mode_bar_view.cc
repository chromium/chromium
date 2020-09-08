// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_bar_view.h"

#include <memory>

#include "ash/capture_mode/capture_mode_close_button.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_source_view.h"
#include "ash/capture_mode/capture_mode_type_view.h"
#include "ash/style/ash_color_provider.h"
#include "ui/aura/window.h"
#include "ui/views/background.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/platform_style.h"

namespace ash {

namespace {

constexpr gfx::Size kBarSize{328, 64};

constexpr gfx::Insets kBarPadding{/*vertical=*/14, /*horizontal=*/16};

constexpr gfx::RoundedCornersF kBorderRadius{20.f};

constexpr int kSeparatorHeight = 20;

constexpr float kBlurQuality = 0.33f;

// TODO(afakhry): Change this to depend on the height of the Shelf.
constexpr int kDistanceFromScreenBottom = 56;

}  // namespace

CaptureModeBarView::CaptureModeBarView()
    : capture_type_view_(AddChildView(std::make_unique<CaptureModeTypeView>())),
      separator_1_(AddChildView(std::make_unique<views::Separator>())),
      capture_source_view_(
          AddChildView(std::make_unique<CaptureModeSourceView>())),
      separator_2_(AddChildView(std::make_unique<views::Separator>())),
      close_button_(
          AddChildView(std::make_unique<CaptureModeCloseButton>(this))) {
  SetPaintToLayer();
  auto* color_provider = AshColorProvider::Get();
  SkColor background_color = color_provider->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80);
  SetBackground(views::CreateSolidBackground(background_color));
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(kBorderRadius);
  layer()->SetBackgroundBlur(
      static_cast<float>(AshColorProvider::LayerBlurSigma::kBlurDefault));
  layer()->SetBackdropFilterQuality(kBlurQuality);
  auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kBarPadding,
      capture_mode::kBetweenChildSpacing));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  const SkColor separator_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparatorColor);
  separator_1_->SetColor(separator_color);
  separator_1_->SetPreferredHeight(kSeparatorHeight);
  separator_2_->SetColor(separator_color);
  separator_2_->SetPreferredHeight(kSeparatorHeight);
}

CaptureModeBarView::~CaptureModeBarView() = default;

// static
gfx::Rect CaptureModeBarView::GetBounds(aura::Window* root) {
  DCHECK(root);

  auto bounds = root->GetBoundsInRootWindow();
  const int y = bounds.height() - kDistanceFromScreenBottom - kBarSize.height();
  bounds.ClampToCenteredSize(kBarSize);
  bounds.set_y(y);
  return bounds;
}

void CaptureModeBarView::OnCaptureSourceChanged(CaptureModeSource new_source) {
  capture_source_view_->OnCaptureSourceChanged(new_source);
}

void CaptureModeBarView::OnCaptureTypeChanged(CaptureModeType new_type) {
  capture_type_view_->OnCaptureTypeChanged(new_type);
}

const char* CaptureModeBarView::GetClassName() const {
  return "CaptureModeBarView";
}

void CaptureModeBarView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  DCHECK_EQ(sender, close_button_);
  CaptureModeController::Get()->Stop();
}

}  // namespace ash

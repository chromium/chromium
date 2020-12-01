// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_bar_view.h"

#include <memory>

#include "ash/capture_mode/capture_mode_button.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_source_view.h"
#include "ash/capture_mode/capture_mode_type_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/style/ash_color_provider.h"
#include "base/bind.h"
#include "ui/aura/window.h"
#include "ui/views/background.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/style/platform_style.h"

namespace ash {

namespace {

// TODO(crbug.com/1144254): Change this back to {328, 64} when removing the
// feedback button.
constexpr gfx::Size kBarSize{392, 64};

constexpr gfx::Insets kBarPadding{/*vertical=*/14, /*horizontal=*/16};

constexpr gfx::RoundedCornersF kBorderRadius{20.f};

constexpr int kSeparatorHeight = 20;

constexpr float kBlurQuality = 0.33f;

// Distance from the bottom of the bar to the bottom of the display, top of the
// hotseat or top of the shelf depending on the shelf alignment or hotseat
// visibility.
constexpr int kDistanceFromShelfOrHotseatTopDp = 16;

}  // namespace

CaptureModeBarView::CaptureModeBarView()
    : feedback_button_(AddChildView(std::make_unique<CaptureModeButton>(
          base::BindRepeating(&CaptureModeBarView::OnFeedbackButtonPressed,
                              base::Unretained(this)),
          kCaptureModeFeedbackIcon))),
      separator_0_(AddChildView(std::make_unique<views::Separator>())),
      capture_type_view_(AddChildView(std::make_unique<CaptureModeTypeView>())),
      separator_1_(AddChildView(std::make_unique<views::Separator>())),
      capture_source_view_(
          AddChildView(std::make_unique<CaptureModeSourceView>())),
      separator_2_(AddChildView(std::make_unique<views::Separator>())),
      close_button_(AddChildView(std::make_unique<CaptureModeButton>(
          base::BindRepeating(&CaptureModeBarView::OnCloseButtonPressed,
                              base::Unretained(this)),
          kCloseButtonIcon))) {
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
  separator_0_->SetColor(separator_color);
  separator_0_->SetPreferredHeight(kSeparatorHeight);
  separator_1_->SetColor(separator_color);
  separator_1_->SetPreferredHeight(kSeparatorHeight);
  separator_2_->SetColor(separator_color);
  separator_2_->SetPreferredHeight(kSeparatorHeight);
}

CaptureModeBarView::~CaptureModeBarView() = default;

// static
gfx::Rect CaptureModeBarView::GetBounds(aura::Window* root) {
  DCHECK(root);

  auto bounds = root->GetBoundsInScreen();
  int bar_y = bounds.bottom();
  Shelf* shelf = Shelf::ForWindow(root);
  if (shelf->IsHorizontalAlignment()) {
    // Get the widget which has the shelf icons. This is the hotseat widget if
    // the hotseat is extended, shelf widget otherwise.
    const bool hotseat_extended =
        shelf->shelf_layout_manager()->hotseat_state() ==
        HotseatState::kExtended;
    views::Widget* shelf_widget =
        hotseat_extended ? static_cast<views::Widget*>(shelf->hotseat_widget())
                         : static_cast<views::Widget*>(shelf->shelf_widget());
    bar_y = shelf_widget->GetWindowBoundsInScreen().y();
  }

  bar_y -= (kDistanceFromShelfOrHotseatTopDp + kBarSize.height());
  bounds.ClampToCenteredSize(kBarSize);
  bounds.set_y(bar_y);
  return bounds;
}

void CaptureModeBarView::OnFeedbackButtonPressed() {
  auto* controller = CaptureModeController::Get();
  controller->OpenFeedbackDialog();
  controller->Stop();
}

void CaptureModeBarView::OnCaptureSourceChanged(CaptureModeSource new_source) {
  capture_source_view_->OnCaptureSourceChanged(new_source);
}

void CaptureModeBarView::OnCaptureTypeChanged(CaptureModeType new_type) {
  capture_type_view_->OnCaptureTypeChanged(new_type);
  capture_source_view_->OnCaptureTypeChanged(new_type);
}

void CaptureModeBarView::OnCloseButtonPressed() {
  RecordCaptureModeBarButtonType(CaptureModeBarButtonType::kExit);
  CaptureModeController::Get()->Stop();
}

BEGIN_METADATA(CaptureModeBarView, views::View)
END_METADATA

}  // namespace ash

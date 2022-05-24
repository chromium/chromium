// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_bar_view.h"

#include <memory>

#include "ash/capture_mode/capture_mode_button.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_source_view.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/capture_mode/capture_mode_type_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/system_shadow.h"
#include "base/bind.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/controls/separator.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/platform_style.h"

namespace ash {

namespace {

// Full size of capture mode bar view, the width of which will be
// adjusted in projector mode.
constexpr gfx::Size kFullBarSize{376, 64};

constexpr auto kBarPadding = gfx::Insets::VH(14, 16);

constexpr int kBorderRadius = 20;

constexpr int kSeparatorHeight = 20;

// Distance from the bottom of the bar to the bottom of the display, top of the
// hotseat or top of the shelf depending on the shelf alignment or hotseat
// visibility.
constexpr int kDistanceFromShelfOrHotseatTopDp = 16;

}  // namespace

CaptureModeBarView::CaptureModeBarView(bool projector_mode)
    : capture_type_view_(
          AddChildView(std::make_unique<CaptureModeTypeView>(projector_mode))),
      separator_1_(AddChildView(std::make_unique<views::Separator>())),
      capture_source_view_(
          AddChildView(std::make_unique<CaptureModeSourceView>())),
      separator_2_(AddChildView(std::make_unique<views::Separator>())),
      settings_button_(AddChildView(std::make_unique<CaptureModeToggleButton>(
          base::BindRepeating(&CaptureModeBarView::OnSettingsButtonPressed,
                              base::Unretained(this)),
          kCaptureModeSettingsIcon))),
      close_button_(AddChildView(std::make_unique<CaptureModeButton>(
          base::BindRepeating(&CaptureModeBarView::OnCloseButtonPressed,
                              base::Unretained(this)),
          kCaptureModeCloseIcon))),
      shadow_(this,
              SystemShadow::GetElevationFromType(
                  SystemShadow::Type::kElevation12)) {
  SetPaintToLayer();
  auto* color_provider = AshColorProvider::Get();
  SkColor background_color = color_provider->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80);
  SetBackground(views::CreateSolidBackground(background_color));
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(kBorderRadius));
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

  auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kBarPadding,
      capture_mode::kBetweenChildSpacing));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Custom styling for the settings button, which has a dark background and a
  // light colored icon when selected.
  const auto normal_icon = gfx::CreateVectorIcon(
      kCaptureModeSettingsIcon,
      color_provider->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kButtonIconColor));
  settings_button_->SetToggledImage(views::Button::STATE_NORMAL, &normal_icon);
  settings_button_->set_toggled_background_color(
      color_provider->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::
              kControlBackgroundColorInactive));
  settings_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_TOOLTIP_SETTINGS));

  const SkColor separator_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparatorColor);
  separator_1_->SetColor(separator_color);
  separator_1_->SetPreferredLength(kSeparatorHeight);
  separator_2_->SetColor(separator_color);
  separator_2_->SetPreferredLength(kSeparatorHeight);

  close_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));

  if (features::IsDarkLightModeEnabled()) {
    SetBorder(std::make_unique<views::HighlightBorder>(
        kBorderRadius, views::HighlightBorder::Type::kHighlightBorder2,
        /*use_light_colors=*/false));
  }
  shadow_.shadow()->SetShadowStyle(gfx::ShadowStyle::kChromeOSSystemUI);
  shadow_.SetRoundedCornerRadius(kBorderRadius);
}

CaptureModeBarView::~CaptureModeBarView() = default;

// static
gfx::Rect CaptureModeBarView::GetBounds(aura::Window* root,
                                        bool is_in_projector_mode) {
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

  gfx::Size bar_size = kFullBarSize;
  if (is_in_projector_mode) {
    bar_size.set_width(kFullBarSize.width() -
                       capture_mode::kButtonSize.width() -
                       capture_mode::kSpaceBetweenCaptureModeTypeButtons);
  }
  bar_y -= (kDistanceFromShelfOrHotseatTopDp + bar_size.height());
  bounds.ClampToCenteredSize(bar_size);
  bounds.set_y(bar_y);
  return bounds;
}

void CaptureModeBarView::OnCaptureSourceChanged(CaptureModeSource new_source) {
  capture_source_view_->OnCaptureSourceChanged(new_source);
}

void CaptureModeBarView::OnCaptureTypeChanged(CaptureModeType new_type) {
  capture_type_view_->OnCaptureTypeChanged(new_type);
  capture_source_view_->OnCaptureTypeChanged(new_type);
}

void CaptureModeBarView::SetSettingsMenuShown(bool shown) {
  settings_button_->SetToggled(shown);
}

void CaptureModeBarView::OnSettingsButtonPressed() {
  CaptureModeController::Get()->capture_mode_session()->SetSettingsMenuShown(
      !settings_button_->GetToggled());
}

void CaptureModeBarView::OnCloseButtonPressed() {
  RecordCaptureModeBarButtonType(CaptureModeBarButtonType::kExit);
  CaptureModeController::Get()->Stop();
}

BEGIN_METADATA(CaptureModeBarView, views::View)
END_METADATA

}  // namespace ash

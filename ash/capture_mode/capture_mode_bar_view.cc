// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_bar_view.h"

#include <memory>

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ash/capture_mode/capture_mode_source_view.h"
#include "ash/capture_mode/capture_mode_type_view.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/icon_button.h"
#include "ash/style/system_shadow.h"
#include "base/functional/bind.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/controls/separator.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

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

CaptureModeBarView::CaptureModeBarView(CaptureModeBehavior* active_behavior)
    : capture_type_view_(
          AddChildView(std::make_unique<CaptureModeTypeView>(active_behavior))),
      separator_1_(AddChildView(std::make_unique<views::Separator>())),
      capture_source_view_(
          AddChildView(std::make_unique<CaptureModeSourceView>())),
      separator_2_(AddChildView(std::make_unique<views::Separator>())),
      settings_button_(AddChildView(std::make_unique<IconButton>(
          base::BindRepeating(&CaptureModeBarView::OnSettingsButtonPressed,
                              base::Unretained(this)),
          IconButton::Type::kMediumFloating,
          &kCaptureModeSettingsIcon,
          l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_TOOLTIP_SETTINGS),
          /*is_togglable=*/true,
          /*has_border=*/true))),
      close_button_(AddChildView(std::make_unique<IconButton>(
          base::BindRepeating(&CaptureModeBarView::OnCloseButtonPressed,
                              base::Unretained(this)),
          IconButton::Type::kMediumFloating,
          &kCaptureModeCloseIcon,
          l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE),
          /*is_togglable=*/false,
          /*has_border=*/true))),
      shadow_(SystemShadow::CreateShadowOnNinePatchLayerForView(
          this,
          SystemShadow::Type::kElevation12)) {
  SetPaintToLayer();
  SetBackground(views::CreateThemedSolidBackground(kColorAshShieldAndBase80));
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(kBorderRadius));
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

  auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kBarPadding,
      capture_mode::kBetweenChildSpacing));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Customize the settings button toggled color.
  settings_button_->SetIconToggledColorId(kColorAshButtonIconColor);
  settings_button_->SetBackgroundToggledColorId(
      kColorAshControlBackgroundColorInactive);

  // Add highlight helper to settings button and close button.
  CaptureModeSessionFocusCycler::HighlightHelper::Install(settings_button_);
  CaptureModeSessionFocusCycler::HighlightHelper::Install(close_button_);

  separator_1_->SetColorId(ui::kColorAshSystemUIMenuSeparator);
  separator_1_->SetPreferredLength(kSeparatorHeight);
  separator_2_->SetColorId(ui::kColorAshSystemUIMenuSeparator);
  separator_2_->SetPreferredLength(kSeparatorHeight);

  capture_mode_util::SetHighlightBorder(
      this, kBorderRadius,
      chromeos::features::IsJellyrollEnabled()
          ? views::HighlightBorder::Type::kHighlightBorderOnShadow
          : views::HighlightBorder::Type::kHighlightBorder2);

  shadow_->SetRoundedCornerRadius(kBorderRadius);
}

CaptureModeBarView::~CaptureModeBarView() = default;

// static
gfx::Rect CaptureModeBarView::GetBounds(aura::Window* root,
                                        CaptureModeBehavior* active_behavior) {
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
  CHECK(active_behavior);
  if (!active_behavior->ShouldImageCaptureTypeBeAllowed()) {
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

void CaptureModeBarView::OnSettingsButtonPressed(const ui::Event& event) {
  CaptureModeController::Get()->capture_mode_session()->SetSettingsMenuShown(
      !settings_button_->toggled(), /*by_key_event=*/event.IsKeyEvent());
}

void CaptureModeBarView::OnCloseButtonPressed() {
  RecordCaptureModeBarButtonType(CaptureModeBarButtonType::kExit);
  CaptureModeController::Get()->Stop();
}

BEGIN_METADATA(CaptureModeBarView, views::View)
END_METADATA

}  // namespace ash

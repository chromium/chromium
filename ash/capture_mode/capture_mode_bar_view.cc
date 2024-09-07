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
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/style/system_shadow.h"
#include "base/functional/bind.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr auto kBarPadding = gfx::Insets::VH(14, 16);

}  // namespace

CaptureModeBarView::~CaptureModeBarView() = default;

CaptureModeTypeView* CaptureModeBarView::GetCaptureTypeView() const {
  return nullptr;
}

CaptureModeSourceView* CaptureModeBarView::GetCaptureSourceView() const {
  return nullptr;
}

PillButton* CaptureModeBarView::GetStartRecordingButton() const {
  return nullptr;
}

void CaptureModeBarView::OnCaptureSourceChanged(CaptureModeSource new_source) {
  return;
}

void CaptureModeBarView::OnCaptureTypeChanged(CaptureModeType new_type) {
  return;
}

void CaptureModeBarView::SetSettingsMenuShown(bool shown) {
  settings_button_->SetToggled(shown);
}

bool CaptureModeBarView::IsEventOnSettingsButton(
    gfx::Point screen_location) const {
  return settings_button_ &&
         settings_button_->GetBoundsInScreen().Contains(screen_location);
}

void CaptureModeBarView::AddedToWidget() {
  // Since the layer of the shadow has to be added as a sibling to this view's
  // layer, we need to wait until the view is added to the widget.
  auto* parent = layer()->parent();
  parent->Add(shadow_->GetLayer());
  parent->StackAtBottom(shadow_->GetLayer());

  // Make the shadow observe the color provider source change to update the
  // colors.
  shadow_->ObserveColorProviderSource(GetWidget());
}

void CaptureModeBarView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // The shadow layer is a sibling of this view's layer, and should have the
  // same bounds.
  shadow_->SetContentBounds(layer()->bounds());
}

// TODO(hewer): Add a check and/or test so that the behavior sets
// `ShouldShowUserNudge()` to false if the `settings_button_` doesn't exist.
CaptureModeBarView::CaptureModeBarView()
    // Use the `ShadowOnTextureLayer` for the view with fully rounded corners.
    : shadow_(SystemShadow::CreateShadowOnTextureLayer(
          SystemShadow::Type::kElevation12)) {
  SetPaintToLayer();
  SetBackground(views::CreateThemedSolidBackground(kColorAshShieldAndBase80));

  const int border_radius = capture_mode::kCaptureBarHeight / 2;
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(border_radius));
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

  auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kBarPadding,
      capture_mode::kBetweenChildSpacing));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  capture_mode_util::SetHighlightBorder(
      this, border_radius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow);

  shadow_->SetRoundedCornerRadius(border_radius);
}

void CaptureModeBarView::AppendSettingsButton() {
  settings_button_ = AddChildView(std::make_unique<IconButton>(
      base::BindRepeating(&CaptureModeBarView::OnSettingsButtonPressed,
                          base::Unretained(this)),
      IconButton::Type::kMediumFloating, &kCaptureModeSettingsIcon,
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_TOOLTIP_SETTINGS),
      /*is_togglable=*/true,
      /*has_border=*/true));

  // Customize the settings button toggled color.
  settings_button_->SetIconToggledColor(kColorAshButtonIconColor);
  settings_button_->SetBackgroundToggledColor(
      kColorAshControlBackgroundColorInactive);

  // Add highlight helper to the settings button.
  CaptureModeSessionFocusCycler::HighlightHelper::Install(settings_button_);
}

void CaptureModeBarView::AppendCloseButton() {
  close_button_ = AddChildView(std::make_unique<IconButton>(
      base::BindRepeating(&CaptureModeBarView::OnCloseButtonPressed,
                          base::Unretained(this)),
      IconButton::Type::kMediumFloating, &kCaptureModeCloseIcon,
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE),
      /*is_togglable=*/false,
      /*has_border=*/true));

  // Add highlight helper to the close button.
  CaptureModeSessionFocusCycler::HighlightHelper::Install(close_button_);
}

void CaptureModeBarView::OnSettingsButtonPressed(const ui::Event& event) {
  CaptureModeSession* session = static_cast<CaptureModeSession*>(
      CaptureModeController::Get()->capture_mode_session());
  CHECK_EQ(session->session_type(), SessionType::kReal);
  session->SetSettingsMenuShown(!settings_button_->toggled(),
                                /*by_key_event=*/event.IsKeyEvent());
}

void CaptureModeBarView::OnCloseButtonPressed() {
  RecordCaptureModeBarButtonType(CaptureModeBarButtonType::kExit);
  CaptureModeController::Get()->Stop();
}

BEGIN_METADATA(CaptureModeBarView)
END_METADATA

}  // namespace ash

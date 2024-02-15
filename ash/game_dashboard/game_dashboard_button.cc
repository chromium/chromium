// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_button.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/bubble/bubble_utils.h"
#include "ash/game_dashboard/game_dashboard_context.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/rounded_container.h"
#include "ash/style/typography.h"
#include "base/check.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/frame/frame_header.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr float kContainerCornerRadius = 12.0f;
constexpr int kBorderThickness = 1;
constexpr int kIconHeight = 20;
constexpr gfx::Insets kArrowMargins = gfx::Insets::TLBR(0, 6, 0, 0);
constexpr gfx::Insets kButtonBorderInsets = gfx::Insets::TLBR(0, 12, 0, 8);
constexpr gfx::Insets kGamepadIconMargins = gfx::Insets::TLBR(0, 0, 0, 8);

// 8% opacity for button border.
constexpr SkAlpha kAlphaForButtonBorder =
    base::saturated_cast<SkAlpha>(std::numeric_limits<SkAlpha>::max() * 0.08);

ui::ColorId GetBackgroundEnabledColorId(bool is_recording) {
  return is_recording ? cros_tokens::kCrosSysSystemNegativeContainer
                      : cros_tokens::kCrosSysPrimaryContainer;
}

ui::ColorId GetIconAndLabelEnabledColorId(bool is_recording) {
  return is_recording ? cros_tokens::kCrosSysSystemOnNegativeContainer
                      : cros_tokens::kCrosSysOnPrimaryContainer;
}

}  // namespace

GameDashboardButton::GameDashboardButton(PressedCallback callback)
    : views::Button(std::move(callback)) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_inside_border_insets(kButtonBorderInsets);

  SetPaintToLayer();
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(kContainerCornerRadius));
  layer()->SetFillsBoundsOpaquely(false);

  // Add the gamepad icon view.
  gamepad_icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  gamepad_icon_view_->SetProperty(views::kMarginsKey, kGamepadIconMargins);

  // Add the title view.
  title_view_ =
      AddChildView(bubble_utils::CreateLabel(TypographyToken::kCrosButton2));

  // Add the arrow icon view.
  arrow_icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  arrow_icon_view_->SetProperty(views::kMarginsKey, kArrowMargins);
}

GameDashboardButton::~GameDashboardButton() = default;

void GameDashboardButton::SetToggled(bool toggled) {
  if (toggled == toggled_) {
    return;
  }
  toggled_ = toggled;
  UpdateArrowIcon();
}

void GameDashboardButton::OnRecordingStarted() {
  CHECK(!is_recording_);
  is_recording_ = true;
  UpdateViews();
}

void GameDashboardButton::OnRecordingEnded() {
  if (!is_recording_) {
    return;
  }
  is_recording_ = false;
  UpdateViews();
}

void GameDashboardButton::UpdateRecordingDuration(
    const std::u16string& duration) {
  DCHECK(title_view_);
  SetTitle(l10n_util::GetStringFUTF16(
      IDS_ASH_GAME_DASHBOARD_GAME_DASHBOARD_BUTTON_RECORDING, duration));
}

void GameDashboardButton::AddedToWidget() {
  views::Button::AddedToWidget();
  UpdateViews();
}

void GameDashboardButton::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void GameDashboardButton::OnThemeChanged() {
  views::View::OnThemeChanged();
  // No need to update the theme before this view is added to the widget.
  if (GetWidget()) {
    UpdateViews();
  }
}

void GameDashboardButton::StateChanged(ButtonState old_state) {
  UpdateViews();
}

void GameDashboardButton::UpdateArrowIcon() {
  DCHECK(arrow_icon_view_);
  const gfx::VectorIcon& arrow_icon =
      toggled_ ? kGdButtonUpArrowIcon : kGdButtonDownArrowIcon;
  const SkColor icon_color = GetColorProvider()->GetColor(
      GetEnabled() ? GetIconAndLabelEnabledColorId(is_recording_)
                   : cros_tokens::kCrosSysDisabled);
  arrow_icon_view_->SetImage(
      ui::ImageModel::FromVectorIcon(arrow_icon, icon_color, kIconHeight));
}

void GameDashboardButton::UpdateViews() {
  DCHECK(GetWidget());

  // Don't update `title_view_` because it will be updated by
  // `UpdateRecordingDuration()`.
  if (!is_recording_) {
    SetTitle(l10n_util::GetStringUTF16(
        IDS_ASH_GAME_DASHBOARD_GAME_DASHBOARD_BUTTON_TITLE));
  }

  SetBorder(views::CreateRoundedRectBorder(
      kBorderThickness, kContainerCornerRadius, gfx::Insets(),
      SkColorSetA(DarkLightModeControllerImpl::Get()->IsDarkModeEnabled()
                      ? cros_tokens::kCrosSysWhite
                      : cros_tokens::kCrosSysBlack,
                  kAlphaForButtonBorder)));

  auto* color_provider = GetColorProvider();
  DCHECK(color_provider);

  const bool enabled = GetEnabled();
  SetBackground(views::CreateSolidBackground(color_provider->GetColor(
      enabled ? GetBackgroundEnabledColorId(is_recording_)
              : cros_tokens::kCrosSysDisabledContainer)));

  const SkColor icon_and_label_color = color_provider->GetColor(
      enabled ? GetIconAndLabelEnabledColorId(is_recording_)
              : cros_tokens::kCrosSysDisabled);
  gamepad_icon_view_->SetImage(ui::ImageModel::FromVectorIcon(
      chromeos::kGameDashboardGamepadIcon, icon_and_label_color, kIconHeight));
  title_view_->SetEnabledColor(icon_and_label_color);
  UpdateArrowIcon();
}

void GameDashboardButton::SetTitle(const std::u16string& title_text) {
  SetTooltipText(title_text);
  title_view_->SetText(title_text);
}

BEGIN_METADATA(GameDashboardButton)
END_METADATA

}  // namespace ash

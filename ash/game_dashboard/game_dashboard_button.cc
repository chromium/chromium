// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_button.h"

#include <memory>
#include <string>

#include "ash/bubble/bubble_utils.h"
#include "ash/game_dashboard/game_dashboard_context.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
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

constexpr int kIconHeight = 20;
constexpr gfx::RoundedCornersF kRoundedCornerRadius =
    gfx::RoundedCornersF(12.0f);
constexpr gfx::Insets kButtonBorderInsets = gfx::Insets::TLBR(0, 12, 0, 8);
constexpr gfx::Insets kGamepadIconMargins = gfx::Insets::TLBR(0, 0, 0, 8);
constexpr gfx::Insets kDropdownArrowMargins = gfx::Insets::TLBR(0, 6, 0, 0);

// 30% opacity for disabled state.
constexpr SkAlpha kAlphaForDisabled =
    base::saturated_cast<SkAlpha>(std::numeric_limits<SkAlpha>::max() * 0.3);

ui::ColorId GetBackgroundEnabledColorId(bool is_recording) {
  return is_recording ? cros_tokens::kCrosSysSystemNegativeContainer
                      : cros_tokens::kCrosSysHighlightShape;
}

ui::ColorId GetIconAndLabelEnabledColorId(bool is_recording) {
  return is_recording ? cros_tokens::kCrosSysSystemOnNegativeContainer
                      : cros_tokens::kCrosSysOnPrimaryContainer;
}

SkColor GetColor(ui::ColorProvider* color_provider,
                 ui::ColorId color_id,
                 bool is_enabled) {
  DCHECK(color_provider);

  SkColor color = color_provider->GetColor(color_id);
  return is_enabled ? color : SkColorSetA(color, kAlphaForDisabled);
}

}  // namespace

GameDashboardButton::GameDashboardButton(PressedCallback callback)
    : views::Button(callback) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  SetBorder(views::CreateEmptyBorder(kButtonBorderInsets));
  SetPaintToLayer();
  layer()->SetRoundedCornerRadius(kRoundedCornerRadius);
  layer()->SetFillsBoundsOpaquely(false);

  // Add the gamepad icon view.
  gamepad_icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  gamepad_icon_view_->SetProperty(views::kMarginsKey, kGamepadIconMargins);

  // Add the title view.
  title_view_ = AddChildView(
      bubble_utils::CreateLabel(ash::TypographyToken::kCrosButton2));

  // Add the dropdown icon view.
  dropdown_icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  dropdown_icon_view_->SetProperty(views::kMarginsKey, kDropdownArrowMargins);
}

GameDashboardButton::~GameDashboardButton() = default;

void GameDashboardButton::SetToggled(bool toggled) {
  if (toggled == toggled_) {
    return;
  }
  toggled_ = toggled;
  UpdateDropDownArrow();
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

void GameDashboardButton::UpdateDropDownArrow() {
  DCHECK(dropdown_icon_view_);
  const gfx::VectorIcon& dropdown_icon =
      toggled_ ? kGdDropUpArrowIcon : kGdDropDownArrowIcon;
  const SkColor icon_color =
      GetColor(GetColorProvider(), GetIconAndLabelEnabledColorId(is_recording_),
               GetEnabled());
  dropdown_icon_view_->SetImage(
      ui::ImageModel::FromVectorIcon(dropdown_icon, icon_color, kIconHeight));
}

void GameDashboardButton::UpdateViews() {
  DCHECK(GetWidget());

  // Don't update `title_view_` because it will be updated by
  // `UpdateRecordingDuration()`.
  if (!is_recording_) {
    SetTitle(l10n_util::GetStringUTF16(
        IDS_ASH_GAME_DASHBOARD_GAME_DASHBOARD_BUTTON_TITLE));
  }

  auto* color_provider = GetColorProvider();
  DCHECK(color_provider);

  const bool enabled = GetEnabled();
  SetBackground(views::CreateSolidBackground(GetColor(
      color_provider, GetBackgroundEnabledColorId(is_recording_), enabled)));

  const SkColor icon_and_label_color = GetColor(
      color_provider, GetIconAndLabelEnabledColorId(is_recording_), enabled);
  gamepad_icon_view_->SetImage(ui::ImageModel::FromVectorIcon(
      chromeos::kGameDashboardGamepadIcon, icon_and_label_color, kIconHeight));
  title_view_->SetEnabledColor(icon_and_label_color);
  UpdateDropDownArrow();
}

void GameDashboardButton::SetTitle(const std::u16string& title_text) {
  SetTooltipText(title_text);
  title_view_->SetText(title_text);
}

BEGIN_METADATA(GameDashboardButton, views::Button)
END_METADATA

}  // namespace ash

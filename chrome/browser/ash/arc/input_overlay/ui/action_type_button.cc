// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_type_button.h"

#include "ash/style/ash_color_id.h"
#include "ash/style/style_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace arc::input_overlay {

namespace {

constexpr int kCornerRadius = 16;
constexpr int kButtonWidth = 110;
constexpr int kActionTypeButtonHeight = 94;
constexpr int kActionTypeIconSize = 48;
constexpr int kLabelIconSpacing = 8;
constexpr int kTopSpacing = 10;
constexpr int kBorderThickness = 1;

// Gap between focus ring outer edge to label.
constexpr float kHaloInset = -5;
// Thickness of focus ring.
constexpr float kHaloThickness = 3;

}  // namespace

ActionTypeButton::ActionTypeButton(PressedCallback callback,
                                   const std::u16string& label,
                                   const gfx::VectorIcon& icon)
    : ash::OptionButtonBase(kButtonWidth,
                            std::move(callback),
                            label,
                            gfx::Insets::VH(10, 12)),
      icon_(icon) {
  SetPreferredSize(gfx::Size(kButtonWidth, kActionTypeButtonHeight));
  SetVisible(true);
  SetBackground(views::CreateRoundedRectBackground(SK_ColorTRANSPARENT,
                                                   /*radius=*/kCornerRadius));
  SetBorder(views::CreateThemedRoundedRectBorder(
      /*thickness=*/kBorderThickness,
      /*radius=*/kCornerRadius, cros_tokens::kCrosSysHoverOnSubtle));

  // Set highlight path.
  views::HighlightPathGenerator::Install(
      this, std::make_unique<views::RoundRectHighlightPathGenerator>(
                gfx::Insets(), /*corner_radius=*/kCornerRadius));
  ash::StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                        /*highlight_on_hover=*/true,
                                        /*highlight_on_focus=*/false);
  // Set focus ring style.
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(ui::kColorAshFocusRing);
  focus_ring->SetHaloInset(kHaloInset);
  focus_ring->SetHaloThickness(kHaloThickness);
}

ActionTypeButton::~ActionTypeButton() = default;

void ActionTypeButton::Layout() {
  SizeToPreferredSize();
  gfx::Rect local_bounds = GetLocalBounds();
  gfx::Rect local_content_bounds(local_bounds);
  local_content_bounds.Inset(GetInsets());

  ink_drop_container()->SetBoundsRect(local_bounds);

  views::Label* label = this->label();
  gfx::Size label_size(label->GetPreferredSize().width(),
                       label->GetPreferredSize().height());

  gfx::Point image_origin = local_content_bounds.origin();
  image_origin.Offset((local_content_bounds.width() - kActionTypeIconSize) / 2,
                      kTopSpacing);
  gfx::Point label_origin = local_content_bounds.origin();
  label_origin.Offset((local_content_bounds.width() - label_size.width()) / 2,
                      kTopSpacing + kActionTypeIconSize + kLabelIconSpacing);

  image()->SetBoundsRect(gfx::Rect(
      image_origin, gfx::Size(kActionTypeIconSize, kActionTypeIconSize)));
  label->SetBoundsRect(gfx::Rect(label_origin, label_size));
  Button::Layout();
}

gfx::ImageSkia ActionTypeButton::GetImage(ButtonState for_state) const {
  return gfx::CreateVectorIcon(GetVectorIcon(), kActionTypeIconSize,
                               GetIconImageColor());
}

const gfx::VectorIcon& ActionTypeButton::GetVectorIcon() const {
  return *icon_;
}

bool ActionTypeButton::IsIconOnTheLeftSide() {
  return false;
}

gfx::Size ActionTypeButton::CalculatePreferredSize() const {
  return gfx::Size(kButtonWidth, kActionTypeButtonHeight);
}

void ActionTypeButton::OnThemeChanged() {
  views::Button::OnThemeChanged();
  UpdateImage();
  RefreshColors();
}

void ActionTypeButton::RefreshColors() {
  const bool is_selected = selected();
  auto active_color_id = is_selected ? cros_tokens::kCrosSysPrimary
                                     : cros_tokens::kCrosSysOnSurface;
  auto disabled_color_id = is_selected
                               ? ash::kColorAshIconPrimaryDisabledColor
                               : ash::kColorAshIconSecondaryDisabledColor;
  SetEnabledTextColorIds(active_color_id);
  SetTextColorId(ButtonState::STATE_DISABLED, disabled_color_id);
  SetBackground(is_selected ? views::CreateThemedRoundedRectBackground(
                                  cros_tokens::kCrosSysHighlightShape,
                                  /*radius=*/kCornerRadius)
                            : views::CreateRoundedRectBackground(
                                  SK_ColorTRANSPARENT,
                                  /*radius=*/kCornerRadius));
  SetBorder(is_selected
                ? views::CreateEmptyBorder(/*thickness=*/kBorderThickness)
                : views::CreateThemedRoundedRectBorder(
                      /*thickness=*/kBorderThickness,
                      /*radius=*/kCornerRadius,
                      cros_tokens::kCrosSysHoverOnSubtle));
}

}  // namespace arc::input_overlay

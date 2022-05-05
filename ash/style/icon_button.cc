// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/icon_button.h"

#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "ash/utility/haptics_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {
namespace {

constexpr int kSmallButtonSize = 32;
constexpr int kMediumButtonSize = 36;
constexpr int kLargeButtonSize = 48;

constexpr int kBorderSize = 4;

// Icon size of the IconButton. Though the button has different sizes, the icon
// inside will be kept the same size.
constexpr int kIconSize = 20;

// The gap between the focus ring and the button's content.
constexpr gfx::Insets kFocusRingPadding(1);

int GetButtonSizeOnType(IconButton::Type type) {
  switch (type) {
    case IconButton::Type::kSmall:
    case IconButton::Type::kSmallFloating:
      return kSmallButtonSize;
    case IconButton::Type::kMedium:
    case IconButton::Type::kMediumFloating:
      return kMediumButtonSize;
    case IconButton::Type::kLarge:
    case IconButton::Type::kLargeFloating:
      return kLargeButtonSize;
  }
}

bool IsFloatingIconButton(IconButton::Type type) {
  return type == IconButton::Type::kSmallFloating ||
         type == IconButton::Type::kMediumFloating ||
         type == IconButton::Type::kLargeFloating;
}

}  // namespace

IconButton::IconButton(PressedCallback callback,
                       IconButton::Type type,
                       const gfx::VectorIcon* icon,
                       int accessible_name_id)
    : IconButton(std::move(callback),
                 type,
                 icon,
                 accessible_name_id,
                 /*is_togglable=*/false,
                 /*has_border=*/false) {}

IconButton::IconButton(PressedCallback callback,
                       IconButton::Type type,
                       const gfx::VectorIcon* icon,
                       bool is_togglable,
                       bool has_border)
    : views::ImageButton(std::move(callback)),
      type_(type),
      icon_(icon),
      is_togglable_(is_togglable) {
  int button_size = GetButtonSizeOnType(type);
  if (has_border) {
    button_size += 2 * kBorderSize;
    SetBorder(views::CreateEmptyBorder(kBorderSize));
  }
  SetPreferredSize(gfx::Size(button_size, button_size));

  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);
  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false);
  if (has_border) {
    // The focus ring will be around the whole button's bounds, but the inkdrop
    // will have the same size as the content.
    views::FocusRing::Get(this)->SetPathGenerator(
        std::make_unique<views::CircleHighlightPathGenerator>(
            kFocusRingPadding));
    views::InstallCircleHighlightPathGenerator(this, gfx::Insets(kBorderSize));

  } else {
    views::InstallCircleHighlightPathGenerator(this);
  }
}

IconButton::IconButton(PressedCallback callback,
                       IconButton::Type type,
                       const gfx::VectorIcon* icon,
                       const std::u16string& accessible_name,
                       bool is_togglable,
                       bool has_border)
    : IconButton(std::move(callback), type, icon, is_togglable, has_border) {
  SetTooltipText(accessible_name);
}

IconButton::IconButton(PressedCallback callback,
                       IconButton::Type type,
                       const gfx::VectorIcon* icon,
                       int accessible_name_id,
                       bool is_togglable,
                       bool has_border)
    : IconButton(std::move(callback),
                 type,
                 icon,
                 l10n_util::GetStringUTF16(accessible_name_id),
                 is_togglable,
                 has_border) {}

IconButton::~IconButton() = default;

void IconButton::SetVectorIcon(const gfx::VectorIcon& icon) {
  icon_ = &icon;
  UpdateVectorIcon();
}

void IconButton::SetBackgroundColor(const SkColor background_color) {
  if (background_color_ == background_color)
    return;

  background_color_ = background_color;
  SchedulePaint();
}

void IconButton::SetIconColor(const SkColor icon_color) {
  if (icon_color_ == icon_color)
    return;
  icon_color_ = icon_color;
  UpdateVectorIcon();
}

void IconButton::SetToggled(bool toggled) {
  if (!is_togglable_ || toggled_ == toggled)
    return;

  toggled_ = toggled;
  UpdateVectorIcon();
}

void IconButton::PaintButtonContents(gfx::Canvas* canvas) {
  if (!IsFloatingIconButton(type_)) {
    const gfx::Rect rect(GetContentsBounds());
    cc::PaintFlags flags;
    flags.setAntiAlias(true);

    const auto* color_provider = AshColorProvider::Get();
    SkColor color = color_provider->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
    bool should_show_button_toggled_on =
        toggled_ &&
        (GetEnabled() ||
         button_behavior_ ==
             DisabledButtonBehavior::kCanDisplayDisabledToggleValue);
    if (should_show_button_toggled_on) {
      color = color_provider->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorActive);
    }
    if (background_color_)
      color = background_color_.value();

    // If the button is disabled, apply opacity filter to the color.
    if (!GetEnabled())
      color = AshColorProvider::GetDisabledColor(color);

    flags.setColor(color);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawCircle(gfx::PointF(rect.CenterPoint()), rect.width() / 2,
                       flags);
  }

  views::ImageButton::PaintButtonContents(canvas);
}

void IconButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::ImageButton::GetAccessibleNodeData(node_data);
  node_data->SetName(GetTooltipText(gfx::Point()));
  if (is_togglable_) {
    node_data->role = ax::mojom::Role::kToggleButton;
    node_data->SetCheckedState(toggled_ ? ax::mojom::CheckedState::kTrue
                                        : ax::mojom::CheckedState::kFalse);
  } else {
    node_data->role = ax::mojom::Role::kButton;
  }
}

void IconButton::OnThemeChanged() {
  views::ImageButton::OnThemeChanged();

  UpdateVectorIcon();
  views::FocusRing::Get(this)->SetColor(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kFocusRingColor));
  SchedulePaint();
}

void IconButton::NotifyClick(const ui::Event& event) {
  if (is_togglable_) {
    haptics_util::PlayHapticToggleEffect(
        !toggled_, ui::HapticTouchpadEffectStrength::kMedium);
  }
  views::Button::NotifyClick(event);
}

void IconButton::UpdateVectorIcon() {
  if (!icon_)
    return;

  auto* color_provider = AshColorProvider::Get();
  const SkColor normal_icon_color =
      icon_color_.value_or(color_provider->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kButtonIconColor));
  const SkColor toggled_icon_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColorPrimary);
  const SkColor icon_color = toggled_ ? toggled_icon_color : normal_icon_color;

  // Skip repainting if the incoming icon is the same as the current icon. If
  // the icon has been painted before, |gfx::CreateVectorIcon()| will simply
  // grab the ImageSkia from a cache, so it will be cheap. Note that this
  // assumes that toggled/disabled images changes at the same time as the normal
  // image, which it currently does.
  const gfx::ImageSkia new_normal_image =
      gfx::CreateVectorIcon(*icon_, kIconSize, icon_color);
  const gfx::ImageSkia& old_normal_image =
      GetImage(views::Button::STATE_NORMAL);
  if (!new_normal_image.isNull() && !old_normal_image.isNull() &&
      new_normal_image.BackedBySameObjectAs(old_normal_image)) {
    return;
  }

  SetImage(views::Button::STATE_NORMAL, new_normal_image);
  SetImage(views::Button::STATE_DISABLED,
           gfx::CreateVectorIcon(
               *icon_, kIconSize,
               AshColorProvider::GetDisabledColor(normal_icon_color)));
}

BEGIN_METADATA(IconButton, views::ImageButton)
END_METADATA

}  // namespace ash

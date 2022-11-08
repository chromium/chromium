// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/icon_button.h"

#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/color_util.h"
#include "ash/style/style_util.h"
#include "ash/utility/haptics_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {
namespace {

constexpr int kXSmallButtonSize = 24;
constexpr int kSmallButtonSize = 32;
constexpr int kMediumButtonSize = 36;
constexpr int kLargeButtonSize = 48;

// Icon size of the small, medium and large size buttons.
constexpr int kIconSize = 20;
// Icon size of the extra small size button.
constexpr int kXSmallIconSize = 16;

// The gap between the focus ring and the button's content.
constexpr int kFocusRingPadding = 2;

int GetButtonSizeOnType(IconButton::Type type) {
  switch (type) {
    case IconButton::Type::kXSmall:
    case IconButton::Type::kXSmallFloating:
      return kXSmallButtonSize;
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

int GetIconSizeOnType(IconButton::Type type) {
  if (type == IconButton::Type::kXSmall ||
      type == IconButton::Type::kXSmallFloating) {
    return kXSmallIconSize;
  }
  return kIconSize;
}

bool IsFloatingIconButton(IconButton::Type type) {
  return type == IconButton::Type::kXSmallFloating ||
         type == IconButton::Type::kSmallFloating ||
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
  const int button_size = GetButtonSizeOnType(type);
  SetPreferredSize(gfx::Size(button_size, button_size));

  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);
  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false);

  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(ui::kColorAshFocusRing);
  if (has_border) {
    // The focus ring will have the outline padding with the bounds of the
    // buttons.
    focus_ring->SetPathGenerator(
        std::make_unique<views::CircleHighlightPathGenerator>(-gfx::Insets(
            focus_ring->GetHaloThickness() / 2 + kFocusRingPadding)));
  }

  views::InstallCircleHighlightPathGenerator(this);
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

void IconButton::SetBackgroundToggledColor(
    const SkColor background_toggled_color) {
  if (!is_togglable_ || background_toggled_color == background_toggled_color_)
    return;

  background_toggled_color_ = background_toggled_color;
  SchedulePaint();
}

void IconButton::SetBackgroundColorId(ui::ColorId background_color_id) {
  if (background_color_id_ == background_color_id)
    return;

  background_color_id_ = background_color_id;
  SchedulePaint();
}

void IconButton::SetBackgroundToggledColorId(
    ui::ColorId background_toggled_color_id) {
  if (!is_togglable_ ||
      background_toggled_color_id == background_toggled_color_id_) {
    return;
  }

  background_toggled_color_id_ = background_toggled_color_id;
  SchedulePaint();
}

void IconButton::SetBackgroundImage(const gfx::ImageSkia& background_image) {
  background_image_ = gfx::ImageSkiaOperations::CreateResizedImage(
      background_image, skia::ImageOperations::RESIZE_BEST, GetPreferredSize());
  SchedulePaint();
}

void IconButton::SetIconColor(const SkColor icon_color) {
  if (icon_color_ == icon_color)
    return;
  icon_color_ = icon_color;
  UpdateVectorIcon();
}

void IconButton::SetIconToggledColor(const SkColor icon_toggled_color) {
  if (!is_togglable_ || icon_toggled_color == icon_toggled_color_)
    return;

  icon_toggled_color_ = icon_toggled_color;
  UpdateVectorIcon();
}

void IconButton::SetIconColorId(ui::ColorId icon_color_id) {
  if (icon_color_id_ == icon_color_id)
    return;
  icon_color_id_ = icon_color_id;
  UpdateVectorIcon();
}

void IconButton::SetIconToggledColorId(ui::ColorId icon_toggled_color_id) {
  if (!is_togglable_ || icon_toggled_color_id == icon_toggled_color_id_)
    return;

  icon_toggled_color_id_ = icon_toggled_color_id;
  UpdateVectorIcon();
}

void IconButton::SetIconSize(int size) {
  if (icon_size_ == size)
    return;
  icon_size_ = size;
  UpdateVectorIcon();
}

void IconButton::SetToggled(bool toggled) {
  if (!is_togglable_ || toggled_ == toggled)
    return;

  toggled_ = toggled;

  if (delegate_)
    delegate_->OnButtonToggled(this);

  UpdateVectorIcon();
}

void IconButton::PaintButtonContents(gfx::Canvas* canvas) {
  if (!GetWidget())
    return;

  const bool toggled_on =
      toggled_ && (GetEnabled() ||
                   button_behavior_ ==
                       DisabledButtonBehavior::kCanDisplayDisabledToggleValue);
  if (!IsFloatingIconButton(type_) || toggled_on) {
    const gfx::Rect rect(GetContentsBounds());
    cc::PaintFlags flags;
    flags.setAntiAlias(true);

    const bool is_jellyroll_enabled = features::IsJellyrollEnabled();
    auto* color_provider = GetColorProvider();

    // The background color IDs set by clients takes precedence over the
    // background colors. If neither is set, use the default color IDs.
    SkColor normal_background_color;
    if (background_color_id_) {
      normal_background_color =
          color_provider->GetColor(background_toggled_color_id_.value());
    } else {
      normal_background_color =
          background_color_.value_or(color_provider->GetColor(
              is_jellyroll_enabled
                  ? cros_tokens::kCrosSysSystemOnBase
                  : static_cast<ui::ColorId>(
                        kColorAshControlBackgroundColorInactive)));
    }

    SkColor toggled_background_color;
    if (background_toggled_color_id_) {
      toggled_background_color =
          color_provider->GetColor(background_toggled_color_id_.value());
    } else {
      toggled_background_color =
          background_toggled_color_.value_or(color_provider->GetColor(
              is_jellyroll_enabled
                  ? cros_tokens::kCrosSysSystemPrimaryContainer
                  : static_cast<ui::ColorId>(
                        kColorAshControlBackgroundColorActive)));
    }

    SkColor color =
        toggled_on ? toggled_background_color : normal_background_color;

    // If the button is disabled, apply opacity filter to the color.
    if (!GetEnabled())
      color = ColorUtil::GetDisabledColor(color);

    flags.setColor(color);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawCircle(gfx::PointF(rect.CenterPoint()), rect.width() / 2,
                       flags);

    // Apply the background image. This is painted on top of the |color|.
    if (!background_image_.isNull()) {
      SkPath mask;
      mask.addCircle(rect.CenterPoint().x(), rect.CenterPoint().y(),
                     rect.width() / 2);
      canvas->ClipPath(mask, true);
      canvas->DrawImageInt(background_image_, 0, 0, flags);
    }
  }

  views::ImageButton::PaintButtonContents(canvas);
}

void IconButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::ImageButton::GetAccessibleNodeData(node_data);
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
  SchedulePaint();
}

void IconButton::NotifyClick(const ui::Event& event) {
  if (is_togglable_) {
    haptics_util::PlayHapticToggleEffect(
        !toggled_, ui::HapticTouchpadEffectStrength::kMedium);
  }

  if (delegate_)
    delegate_->OnButtonClicked(this);

  views::Button::NotifyClick(event);
}

void IconButton::UpdateVectorIcon() {
  if (!icon_ || !GetWidget())
    return;

  auto* color_provider = GetColorProvider();
  const bool is_jellyroll_enabled = features::IsJellyrollEnabled();

  // The icon color IDs set by clients takes precedence over the icon colors. If
  // neither is set, use the default color IDs.
  SkColor normal_icon_color;
  if (icon_color_id_) {
    normal_icon_color = color_provider->GetColor(icon_color_id_.value());
  } else {
    normal_icon_color = icon_color_.value_or(color_provider->GetColor(
        is_jellyroll_enabled
            ? cros_tokens::kCrosSysOnSurface
            : static_cast<ui::ColorId>(kColorAshButtonIconColor)));
  }

  SkColor toggled_icon_color;
  if (icon_toggled_color_id_) {
    toggled_icon_color =
        color_provider->GetColor(icon_toggled_color_id_.value());
  } else {
    toggled_icon_color = icon_toggled_color_.value_or(color_provider->GetColor(
        is_jellyroll_enabled
            ? cros_tokens::kCrosSysSystemOnPrimaryContainer
            : static_cast<ui::ColorId>(kColorAshButtonIconColorPrimary)));
  }

  const SkColor icon_color = toggled_ ? toggled_icon_color : normal_icon_color;
  const int icon_size = icon_size_.value_or(GetIconSizeOnType(type_));

  // Skip repainting if the incoming icon is the same as the current icon. If
  // the icon has been painted before, |gfx::CreateVectorIcon()| will simply
  // grab the ImageSkia from a cache, so it will be cheap. Note that this
  // assumes that toggled/disabled images changes at the same time as the normal
  // image, which it currently does.
  const gfx::ImageSkia new_normal_image =
      gfx::CreateVectorIcon(*icon_, icon_size, icon_color);
  const gfx::ImageSkia& old_normal_image =
      GetImage(views::Button::STATE_NORMAL);
  if (!new_normal_image.isNull() && !old_normal_image.isNull() &&
      new_normal_image.BackedBySameObjectAs(old_normal_image)) {
    return;
  }

  SetImage(views::Button::STATE_NORMAL, new_normal_image);
  SetImage(
      views::Button::STATE_DISABLED,
      gfx::CreateVectorIcon(*icon_, icon_size,
                            ColorUtil::GetDisabledColor(normal_icon_color)));
}

BEGIN_METADATA(IconButton, views::ImageButton)
END_METADATA

}  // namespace ash

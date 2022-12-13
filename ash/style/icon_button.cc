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
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {
namespace {

constexpr int kXSmallButtonSize = 20;
constexpr int kSmallButtonSize = 24;
constexpr int kMediumButtonSize = 32;
constexpr int kLargeButtonSize = 36;

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
    case IconButton::Type::kXSmallProminentFloating:
      return kXSmallButtonSize;
    case IconButton::Type::kSmall:
    case IconButton::Type::kSmallFloating:
    case IconButton::Type::kSmallProminentFloating:
      return kSmallButtonSize;
    case IconButton::Type::kMedium:
    case IconButton::Type::kMediumFloating:
    case IconButton::Type::kMediumProminentFloating:
      return kMediumButtonSize;
    case IconButton::Type::kLarge:
    case IconButton::Type::kLargeFloating:
    case IconButton::Type::kLargeProminentFloating:
      return kLargeButtonSize;
  }
}

int GetIconSizeOnType(IconButton::Type type) {
  if (type == IconButton::Type::kXSmall ||
      type == IconButton::Type::kXSmallFloating ||
      type == IconButton::Type::kXSmallProminentFloating) {
    return kXSmallIconSize;
  }
  return kIconSize;
}

bool IsFloatingIconButton(IconButton::Type type) {
  switch (type) {
    case IconButton::Type::kXSmallFloating:
    case IconButton::Type::kXSmallProminentFloating:
    case IconButton::Type::kSmallFloating:
    case IconButton::Type::kSmallProminentFloating:
    case IconButton::Type::kMediumFloating:
    case IconButton::Type::kMediumProminentFloating:
    case IconButton::Type::kLargeFloating:
    case IconButton::Type::kLargeProminentFloating:
      return true;
    default:
      break;
  }

  return false;
}

bool IsProminentFloatingType(IconButton::Type type) {
  switch (type) {
    case IconButton::Type::kXSmallProminentFloating:
    case IconButton::Type::kSmallProminentFloating:
    case IconButton::Type::kMediumProminentFloating:
    case IconButton::Type::kLargeProminentFloating:
      return true;
    default:
      break;
  }

  return false;
}

// Create a themed fully rounded rect background for icon button.
std::unique_ptr<views::Background> CreateThemedBackground(
    ui::ColorId color_id,
    IconButton::Type type) {
  return views::CreateThemedRoundedRectBackground(
      color_id, GetButtonSizeOnType(type) / 2);
}

// Create a solid color fully rounded rect background for icon button.
// TODO(zxdan): Remove this function when the dynamic color migration work is
// done.
std::unique_ptr<views::Background> CreateSolidBackground(
    SkColor color,
    IconButton::Type type) {
  return views::CreateRoundedRectBackground(color,
                                            GetButtonSizeOnType(type) / 2);
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

  UpdateBackground();
  UpdateVectorIcon(/*icon_changed=*/true);

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

  enabled_changed_subscription_ = AddEnabledChangedCallback(base::BindRepeating(
      &IconButton::UpdateBackground, base::Unretained(this)));
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
  UpdateVectorIcon(/*icon_changed=*/true);
}

void IconButton::SetToggledVectorIcon(const gfx::VectorIcon& icon) {
  toggled_icon_ = &icon;
  UpdateVectorIcon();
}

void IconButton::SetBackgroundColor(const SkColor background_color) {
  if (background_color_ == background_color)
    return;

  background_color_ = background_color;
  background_color_id_ = absl::nullopt;

  if (GetEnabled() && !IsToggledOn())
    UpdateBackground();
}

void IconButton::SetBackgroundToggledColor(
    const SkColor background_toggled_color) {
  if (!is_togglable_ || background_toggled_color == background_toggled_color_)
    return;

  background_toggled_color_ = background_toggled_color;
  background_toggled_color_id_ = absl::nullopt;

  if (GetEnabled() && IsToggledOn())
    UpdateBackground();
}

void IconButton::SetBackgroundColorId(ui::ColorId background_color_id) {
  if (background_color_id_ == background_color_id)
    return;

  background_color_id_ = background_color_id;
  background_color_ = absl::nullopt;

  if (GetEnabled() && !IsToggledOn())
    UpdateBackground();
}

void IconButton::SetBackgroundToggledColorId(
    ui::ColorId background_toggled_color_id) {
  if (!is_togglable_ ||
      background_toggled_color_id == background_toggled_color_id_) {
    return;
  }

  background_toggled_color_id_ = background_toggled_color_id;
  background_toggled_color_ = absl::nullopt;

  if (GetEnabled() && IsToggledOn())
    UpdateBackground();
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
  icon_color_id_ = absl::nullopt;

  if (!IsToggledOn())
    UpdateVectorIcon();
}

void IconButton::SetIconToggledColor(const SkColor icon_toggled_color) {
  if (!is_togglable_ || icon_toggled_color == icon_toggled_color_)
    return;

  icon_toggled_color_ = icon_toggled_color;
  icon_toggled_color_id_ = absl::nullopt;

  if (IsToggledOn())
    UpdateVectorIcon();
}

void IconButton::SetIconColorId(ui::ColorId icon_color_id) {
  if (icon_color_id_ == icon_color_id)
    return;

  icon_color_id_ = icon_color_id;
  icon_color_ = absl::nullopt;

  if (!IsToggledOn())
    UpdateVectorIcon();
}

void IconButton::SetIconToggledColorId(ui::ColorId icon_toggled_color_id) {
  if (!is_togglable_ || icon_toggled_color_id == icon_toggled_color_id_)
    return;

  icon_toggled_color_id_ = icon_toggled_color_id;
  icon_toggled_color_ = absl::nullopt;

  if (IsToggledOn())
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

  if (GetEnabled())
    UpdateBackground();

  UpdateVectorIcon();
}

void IconButton::OnFocus() {
  // Update prominent floating type button's icon color on focus.
  if (IsProminentFloatingType(type_) && !IsToggledOn())
    UpdateVectorIcon();
}

void IconButton::OnBlur() {
  // Update prominent floating type button's icon color on blur.
  if (IsProminentFloatingType(type_) && !IsToggledOn())
    UpdateVectorIcon();
}

void IconButton::PaintButtonContents(gfx::Canvas* canvas) {
  if (!IsFloatingIconButton(type_) || IsToggledOn()) {
    // Apply the background image. This is painted on top of the |color|.
    if (!background_image_.isNull()) {
      const gfx::Rect rect(GetContentsBounds());
      cc::PaintFlags flags;
      flags.setAntiAlias(true);
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

void IconButton::NotifyClick(const ui::Event& event) {
  if (is_togglable_) {
    haptics_util::PlayHapticToggleEffect(
        !toggled_, ui::HapticTouchpadEffectStrength::kMedium);
  }

  if (delegate_)
    delegate_->OnButtonClicked(this);

  views::Button::NotifyClick(event);
}

void IconButton::UpdateBackground() {
  // The untoggled floating button does not have a background.
  const bool is_toggled = IsToggledOn();
  if (IsFloatingIconButton(type_) && !is_toggled) {
    SetBackground(nullptr);
    return;
  }

  // Create a themed rounded rect background when the button is disabled.
  if (!GetEnabled()) {
    SetBackground(
        CreateThemedBackground(cros_tokens::kCrosSysDisabledContainer, type_));
    return;
  }

  // Create a themed rounded rect background when the background color is
  // defined by a color ID. Otherwise, create a solid rounded rect background.
  // TODO(zxdan): only use themed background when dynamic color migration work
  // is done.

  // When the button is toggled, create a background with toggled color.
  const bool is_jellyroll_enabled = features::IsJellyrollEnabled();
  if (is_toggled) {
    if (background_toggled_color_id_ || !background_toggled_color_) {
      const ui::ColorId color_id = background_toggled_color_id_.value_or(
          is_jellyroll_enabled ? cros_tokens::kCrosSysSystemPrimaryContainer
                               : static_cast<ui::ColorId>(
                                     kColorAshControlBackgroundColorActive));
      SetBackground(CreateThemedBackground(color_id, type_));
      return;
    }
    SetBackground(
        CreateSolidBackground(background_toggled_color_.value(), type_));
    return;
  }

  // When the button is not toggled, create a background with normal color.
  if (background_color_id_ || !background_color_) {
    const ui::ColorId color_id = background_color_id_.value_or(
        is_jellyroll_enabled ? cros_tokens::kCrosSysSystemOnBase
                             : static_cast<ui::ColorId>(
                                   kColorAshControlBackgroundColorInactive));
    SetBackground(CreateThemedBackground(color_id, type_));
    return;
  }
  SetBackground(CreateSolidBackground(background_color_.value(), type_));
  return;
}

void IconButton::UpdateVectorIcon(bool icon_changed) {
  const bool is_toggled = IsToggledOn();
  const gfx::VectorIcon* icon =
      is_toggled && toggled_icon_ ? toggled_icon_ : icon_;

  if (!icon)
    return;

  const int icon_size = icon_size_.value_or(GetIconSizeOnType(type_));
  const bool is_jellyroll_enabled = features::IsJellyrollEnabled();

  ui::ImageModel new_normal_image_model;
  // When the icon color is defined by a color Id, use the color Id to create an
  // image model. Otherwise, use the color to create an image model.
  // TODO(zxdan): only use color Id when the dynamic color migration work is
  // done.
  if (is_toggled) {
    // When the button is toggled, create an image model with toggled color.
    if (icon_toggled_color_id_ || !icon_toggled_color_) {
      const ui::ColorId color_id = icon_toggled_color_id_.value_or(
          is_jellyroll_enabled
              ? cros_tokens::kCrosSysSystemOnPrimaryContainer
              : static_cast<ui::ColorId>(kColorAshButtonIconColorPrimary));
      new_normal_image_model =
          ui::ImageModel::FromVectorIcon(*icon, color_id, icon_size);
    } else {
      new_normal_image_model = ui::ImageModel::FromVectorIcon(
          *icon, icon_toggled_color_.value(), icon_size);
    }
  } else {
    // When the button is not toggled, create an image model with normal color.
    if (icon_color_id_ || !icon_color_) {
      ui::ColorId default_color_id;
      if (IsProminentFloatingType(type_)) {
        default_color_id = HasFocus() ? cros_tokens::kCrosSysPrimary
                                      : cros_tokens::kCrosSysSecondary;
      } else {
        default_color_id =
            is_jellyroll_enabled
                ? cros_tokens::kCrosSysOnSurface
                : static_cast<ui::ColorId>(kColorAshButtonIconColor);
      }
      const ui::ColorId color_id = icon_color_id_.value_or(default_color_id);
      new_normal_image_model =
          ui::ImageModel::FromVectorIcon(*icon, color_id, icon_size);
    } else {
      new_normal_image_model =
          ui::ImageModel::FromVectorIcon(*icon, icon_color_.value(), icon_size);
    }
  }

  if (GetWidget()) {
    // Skip repainting if the incoming icon is the same as the current icon. If
    // the icon has been painted before, |gfx::CreateVectorIcon()| will simply
    // grab the ImageSkia from a cache, so it will be cheap. Note that this
    // assumes that toggled/disabled images changes at the same time as the
    // normal image, which it currently does.
    const gfx::ImageSkia new_normal_image =
        new_normal_image_model.Rasterize(GetColorProvider());
    const gfx::ImageSkia& old_normal_image =
        GetImage(views::Button::STATE_NORMAL);
    if (!new_normal_image.isNull() && !old_normal_image.isNull() &&
        new_normal_image.BackedBySameObjectAs(old_normal_image)) {
      return;
    }
  }

  SetImageModel(views::Button::STATE_NORMAL, new_normal_image_model);
  if (icon_changed) {
    SetImageModel(views::Button::STATE_DISABLED,
                  ui::ImageModel::FromVectorIcon(
                      *icon, cros_tokens::kCrosSysDisabled, icon_size));
  }
}

SkColor IconButton::GetBackgroundColor() const {
  DCHECK(background());
  return background()->get_color();
}

bool IconButton::IsToggledOn() const {
  return toggled_ &&
         (GetEnabled() ||
          button_behavior_ ==
              DisabledButtonBehavior::kCanDisplayDisabledToggleValue);
}

BEGIN_METADATA(IconButton, views::ImageButton)
END_METADATA

}  // namespace ash

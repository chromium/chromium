// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/pill_button.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/blurred_background_shield.h"
#include "ash/style/color_util.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

namespace {

// The height of default size button, mainly used for button types other than
// kIconLarge.
constexpr int kPillButtonHeight = 32;
// The height of large size button, used for button type kIconLarge.
constexpr int kPillButtonLargeHeight = 36;
constexpr int kPillButtonMinimumWidth = 56;
constexpr int kIconSize = 20;
constexpr int kIconPillButtonImageLabelSpacingDp = 8;

// Including the thickness and inset of the focus ring in order to keep 2px
// padding between the focus ring and content of the button.
constexpr int kFocusRingPadding = 2 + views::FocusRing::kDefaultHaloThickness +
                                  views::FocusRing::kDefaultHaloInset;

// The type mask of button color variant.
// TODO(crbug.com/1355517): Remove `kAccent` from color variant when CrosNext is
// fully launched.
constexpr PillButton::TypeFlag kButtonColorVariant =
    PillButton::kDefault | PillButton::kDefaultElevated | PillButton::kPrimary |
    PillButton::kSecondary | PillButton::kFloating | PillButton::kAlert |
    PillButton::kAccent;

// Returns true it is a floating type of PillButton, which is a type of
// PillButton without a background.
bool IsFloatingPillButton(PillButton::Type type) {
  return type & PillButton::kFloating;
}

// Returns true if the button has an icon.
bool IsIconPillButton(PillButton::Type type) {
  return type & (PillButton::kIconLeading | PillButton::kIconFollowing);
}

// Returns the button height according to the given type.
int GetButtonHeight(PillButton::Type type) {
  return (type & PillButton::kLarge) ? kPillButtonLargeHeight
                                     : kPillButtonHeight;
}

// Checks if the color variant is assigned a color/color ID.
bool IsAssignedColorVariant(PillButton::ColorVariant color_variant) {
  // The color variant is assigned as long as it is not equal to
  // `gfx::kPlaceholderColor`.
  return !(absl::holds_alternative<SkColor>(color_variant) &&
           absl::get<SkColor>(color_variant) == gfx::kPlaceholderColor);
}

// Updates the target color variant with given color variant if they are not
// equal.
bool MaybeUpdateColorVariant(PillButton::ColorVariant& target_color_variant,
                             PillButton::ColorVariant color_variant) {
  if (target_color_variant == color_variant) {
    return false;
  }

  target_color_variant = color_variant;
  return true;
}

std::optional<ui::ColorId> GetDefaultBackgroundColorId(PillButton::Type type) {
  std::optional<ui::ColorId> color_id;

  const bool is_jellyroll_enabled = chromeos::features::IsJellyrollEnabled();

  switch (type & kButtonColorVariant) {
    case PillButton::kDefault:
      color_id = is_jellyroll_enabled
                     ? cros_tokens::kCrosSysSystemOnBase
                     : static_cast<ui::ColorId>(
                           kColorAshControlBackgroundColorInactive);
      break;
    case PillButton::kDefaultElevated:
      color_id = cros_tokens::kCrosSysSystemBaseElevated;
      break;
    case PillButton::kPrimary:
      color_id =
          is_jellyroll_enabled
              ? cros_tokens::kCrosSysPrimary
              : static_cast<ui::ColorId>(kColorAshControlBackgroundColorActive);
      break;
    case PillButton::kSecondary:
      color_id = kColorAshSecondaryButtonBackgroundColor;
      break;
    case PillButton::kAlert:
      color_id =
          is_jellyroll_enabled
              ? cros_tokens::kCrosSysError
              : static_cast<ui::ColorId>(kColorAshControlBackgroundColorAlert);
      break;
    case PillButton::kAccent:
      color_id = kColorAshControlBackgroundColorInactive;
      break;
    default:
      NOTREACHED() << "Invalid and floating pill button type: " << type;
  }

  return color_id;
}

std::optional<ui::ColorId> GetDefaultButtonTextIconColorId(
    PillButton::Type type) {
  std::optional<ui::ColorId> color_id;

  const bool is_jellyroll_enabled = chromeos::features::IsJellyrollEnabled();

  switch (type & kButtonColorVariant) {
    case PillButton::kDefault:
      color_id = is_jellyroll_enabled
                     ? cros_tokens::kCrosSysOnSurface
                     : static_cast<ui::ColorId>(kColorAshButtonLabelColor);
      break;
    case PillButton::kDefaultElevated:
      color_id = cros_tokens::kCrosSysOnSurface;
      break;
    case PillButton::kPrimary:
      color_id =
          is_jellyroll_enabled
              ? cros_tokens::kCrosSysOnPrimary
              : static_cast<ui::ColorId>(kColorAshButtonLabelColorPrimary);
      break;
    case PillButton::kSecondary:
      color_id = cros_tokens::kCrosSysOnSecondaryContainer;
      break;
    case PillButton::kFloating:
      color_id = is_jellyroll_enabled
                     ? cros_tokens::kCrosSysPrimary
                     : static_cast<ui::ColorId>(kColorAshButtonLabelColor);
      break;
    case PillButton::kAlert:
      color_id =
          is_jellyroll_enabled
              ? cros_tokens::kCrosSysOnError
              : static_cast<ui::ColorId>(kColorAshButtonLabelColorPrimary);
      break;
    case PillButton::kAccent:
    case PillButton::kAccent | PillButton::kFloating:
      color_id = kColorAshButtonLabelColorBlue;
      break;
    default:
      NOTREACHED() << "Invalid pill button type: " << type;
  }

  return color_id;
}

}  // namespace

PillButton::PillButton(PressedCallback callback,
                       const std::u16string& text,
                       PillButton::Type type,
                       const gfx::VectorIcon* icon,
                       int horizontal_spacing,
                       int padding_reduction_for_icon)
    : views::LabelButton(std::move(callback), text),
      type_(type),
      icon_(icon),
      horizontal_spacing_(horizontal_spacing),
      padding_reduction_for_icon_(padding_reduction_for_icon) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  label()->SetSubpixelRenderingEnabled(false);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kLegacyButton2,
                                        *label());
  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false,
                                   /*background_color=*/
                                   gfx::kPlaceholderColor);
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetOutsetFocusRingDisabled(true);
  focus_ring->SetColorId(ui::kColorAshFocusRing);

  // Initialize image and icon spacing.
  SetImageLabelSpacing(kIconPillButtonImageLabelSpacingDp);

  Init();

  enabled_changed_subscription_ = AddEnabledChangedCallback(base::BindRepeating(
      &PillButton::UpdateBackgroundColor, base::Unretained(this)));
}

PillButton::~PillButton() = default;

gfx::Size PillButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  int button_width =
      label()
          ->GetPreferredSize(views::SizeBounds(label()->width(), {}))
          .width();

  if (IsIconPillButton(type_)) {
    // Add the padding on two sides.
    button_width += horizontal_spacing_ + GetHorizontalSpacingWithIcon();

    // Add the icon width and the spacing between the icon and the text.
    button_width += kIconSize + GetImageLabelSpacing();
  } else {
    button_width += 2 * horizontal_spacing_;
  }

  const int height = GetButtonHeight(type_);
  gfx::Size size(button_width, height);
  size.SetToMax(gfx::Size(kPillButtonMinimumWidth, height));
  return size;
}

gfx::Insets PillButton::GetInsets() const {
  const int vertical_spacing = (GetButtonHeight(type_) - kIconSize) / 2;
  const int icon_padding = IsIconPillButton(type_)
                               ? GetHorizontalSpacingWithIcon()
                               : horizontal_spacing_;
  if (type_ & kIconFollowing) {
    return gfx::Insets::TLBR(vertical_spacing, horizontal_spacing_,
                             vertical_spacing, icon_padding);
  }
  return gfx::Insets::TLBR(vertical_spacing, icon_padding, vertical_spacing,
                           horizontal_spacing_);
}

void PillButton::UpdateBackgroundColor() {
  if (IsFloatingPillButton(type_)) {
    return;
  }

  // Resolve the expected background color.
  ColorVariant background_color;
  if (!GetEnabled()) {
    background_color = cros_tokens::kCrosSysDisabledContainer;
  } else if (IsAssignedColorVariant(background_color_)) {
    background_color = background_color_;
  } else {
    auto default_color_id = GetDefaultBackgroundColorId(type_);
    DCHECK(default_color_id);
    background_color = default_color_id.value();
  }

  // Replace the background with blurred background shield if the background
  // blur is enabled. Otherwise, remove the blurred background shield.
  const float corner_radius = GetButtonHeight(type_) / 2.0f;
  if (enable_background_blur_) {
    if (background()) {
      SetBackground(nullptr);
    }

    if (!blurred_background_) {
      blurred_background_ = std::make_unique<BlurredBackgroundShield>(
          this, background_color, ColorProvider::kBackgroundBlurSigma,
          gfx::RoundedCornersF(corner_radius),
          /*add_layer_to_region=*/false);
      return;
    }
  } else if (blurred_background_) {
    blurred_background_.reset();
  }

  // Create the background with expected color or update the colors of blurred
  // background shield.
  if (absl::holds_alternative<SkColor>(background_color)) {
    SkColor color_value = absl::get<SkColor>(background_color);
    if (enable_background_blur_) {
      blurred_background_->SetColor(color_value);
    } else {
      SetBackground(
          views::CreateRoundedRectBackground(color_value, corner_radius));
    }
  } else {
    ui::ColorId color_id = absl::get<ui::ColorId>(background_color);
    if (enable_background_blur_) {
      blurred_background_->SetColorId(color_id);
    } else {
      SetBackground(
          views::CreateThemedRoundedRectBackground(color_id, corner_radius));
    }
  }
}

views::PropertyEffects PillButton::UpdateStyleToIndicateDefaultStatus() {
  // Override the method defined in LabelButton to avoid style changes when the
  // `is_default_` flag is updated.
  return views::kPropertyEffectsNone;
}

std::u16string PillButton::GetTooltipText(const gfx::Point& p) const {
  const auto& tooltip = views::LabelButton::GetTooltipText(p);
  if (use_label_as_default_tooltip_ && tooltip.empty()) {
    return GetText();
  }
  return tooltip;
}

void PillButton::SetBackgroundColor(const SkColor background_color) {
  if (MaybeUpdateColorVariant(background_color_, background_color)) {
    UpdateBackgroundColor();
  }
}

void PillButton::SetBackgroundColorId(ui::ColorId background_color_id) {
  if (MaybeUpdateColorVariant(background_color_, background_color_id)) {
    UpdateBackgroundColor();
  }
}

void PillButton::SetButtonTextColor(const SkColor text_color) {
  if (MaybeUpdateColorVariant(text_color_, text_color)) {
    UpdateTextColor();
  }
}

void PillButton::SetButtonTextColorId(ui::ColorId text_color_id) {
  if (MaybeUpdateColorVariant(text_color_, text_color_id)) {
    UpdateTextColor();
  }
}

void PillButton::SetIconColor(const SkColor icon_color) {
  if (MaybeUpdateColorVariant(icon_color_, icon_color)) {
    UpdateIconColor();
  }
}

void PillButton::SetIconColorId(ui::ColorId icon_color_id) {
  if (MaybeUpdateColorVariant(icon_color_, icon_color_id)) {
    UpdateIconColor();
  }
}

void PillButton::SetPillButtonType(Type type) {
  if (type_ == type)
    return;

  type_ = type;
  Init();
}

void PillButton::SetUseDefaultLabelFont() {
  label()->SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
      TypographyToken::kLegacyBody2));
}

void PillButton::SetEnableBackgroundBlur(bool enable) {
  if (enable_background_blur_ == enable) {
    return;
  }

  enable_background_blur_ = enable;
  UpdateBackgroundColor();
}

void PillButton::SetTextWithStringId(int message_id) {
  SetText(l10n_util::GetStringUTF16(message_id));
}

void PillButton::SetUseLabelAsDefaultTooltip(
    bool use_label_as_default_tooltip) {
  use_label_as_default_tooltip_ = use_label_as_default_tooltip;
}

void PillButton::Init() {
  if (type_ & kIconFollowing) {
    SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  } else {
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
  }

  const int height = GetButtonHeight(type_);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                height / 2.f);

  if (chromeos::features::IsJellyrollEnabled() ||
      (type_ & kButtonColorVariant) == kPrimary) {
    // Add padding around focus highlight only.
    views::FocusRing::Get(this)->SetPathGenerator(
        std::make_unique<views::RoundRectHighlightPathGenerator>(
            gfx::Insets(-kFocusRingPadding), height / 2.f + kFocusRingPadding));
  }

  // TODO(b/290639214): We no longer need this after deprecating
  // SetPillButtonType since the whether using background should be settled on
  // initialization. For now, we should remove the background if the client
  // changes from non-floating type button to floating type button.
  if (IsFloatingPillButton(type_)) {
    SetBackground(nullptr);
  }

  UpdateBackgroundColor();
  UpdateTextColor();
  UpdateIconColor();

  PreferredSizeChanged();
}

void PillButton::UpdateTextColor() {
  SetTextColorId(views::Button::STATE_DISABLED, cros_tokens::kCrosSysDisabled);

  // If custom text color is set, use it to set text color.
  if (IsAssignedColorVariant(text_color_)) {
    if (absl::holds_alternative<SkColor>(text_color_)) {
      SetEnabledTextColors(absl::get<SkColor>(text_color_));
    } else {
      SetEnabledTextColorIds(absl::get<ui::ColorId>(text_color_));
    }
  } else {
    // Otherwise, use default color ID to set text color.
    auto default_color_id = GetDefaultButtonTextIconColorId(type_);
    DCHECK(default_color_id);
    SetEnabledTextColorIds(default_color_id.value());
  }
}

void PillButton::UpdateIconColor() {
  if (!IsIconPillButton(type_))
    return;

  if (!icon_) {
    return;
  }

  SetImageModel(views::Button::STATE_DISABLED,
                ui::ImageModel::FromVectorIcon(
                    *icon_, cros_tokens::kCrosSysDisabled, kIconSize));

  // If custom icon color is set, use it to set icon color.
  if (IsAssignedColorVariant(icon_color_)) {
    if (absl::holds_alternative<SkColor>(icon_color_)) {
      SetImageModel(views::Button::STATE_NORMAL,
                    ui::ImageModel::FromVectorIcon(
                        *icon_, absl::get<SkColor>(icon_color_), kIconSize));
    } else {
      SetImageModel(
          views::Button::STATE_NORMAL,
          ui::ImageModel::FromVectorIcon(
              *icon_, absl::get<ui::ColorId>(icon_color_), kIconSize));
    }
  } else {
    // Otherwise, use default color ID to set icon color.
    auto default_color_id = GetDefaultButtonTextIconColorId(type_);
    DCHECK(default_color_id);
    SetImageModel(views::Button::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(
                      *icon_, default_color_id.value(), kIconSize));
  }
}

int PillButton::GetHorizontalSpacingWithIcon() const {
  return std::max(horizontal_spacing_ - padding_reduction_for_icon_, 0);
}

BEGIN_METADATA(PillButton)
END_METADATA

}  // namespace ash

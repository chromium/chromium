// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/pill_button.h"

#include <optional>

#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/blurred_background_shield.h"
#include "ash/style/color_util.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
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

// Updates the target color variant with given color variant if they are not
// equal.
bool MaybeUpdateColorVariant(
    std::optional<ui::ColorVariant>& target_color_variant,
    ui::ColorVariant color_variant) {
  if (target_color_variant && target_color_variant == color_variant) {
    return false;
  }

  target_color_variant = color_variant;
  return true;
}

std::optional<ui::ColorId> GetDefaultBackgroundColorId(PillButton::Type type) {
  std::optional<ui::ColorId> color_id;

  switch (type & kButtonColorVariant) {
    case PillButton::kDefault:
      color_id = cros_tokens::kCrosSysSystemOnBase;
      break;
    case PillButton::kDefaultElevated:
      color_id = cros_tokens::kCrosSysSystemBaseElevated;
      break;
    case PillButton::kPrimary:
      color_id = cros_tokens::kCrosSysPrimary;
      break;
    case PillButton::kSecondary:
      color_id = kColorAshSecondaryButtonBackgroundColor;
      break;
    case PillButton::kAlert:
      color_id = cros_tokens::kCrosSysError;
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

  switch (type & kButtonColorVariant) {
    case PillButton::kDefault:
      color_id = cros_tokens::kCrosSysOnSurface;
      break;
    case PillButton::kDefaultElevated:
      color_id = cros_tokens::kCrosSysOnSurface;
      break;
    case PillButton::kPrimary:
      color_id = cros_tokens::kCrosSysOnPrimary;
      break;
    case PillButton::kSecondary:
      color_id = cros_tokens::kCrosSysOnSecondaryContainer;
      break;
    case PillButton::kFloating:
      color_id = cros_tokens::kCrosSysPrimary;
      break;
    case PillButton::kAlert:
      color_id = cros_tokens::kCrosSysOnError;
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
  UpdateTooltipText();
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
  ui::ColorVariant background_color;
  if (!GetEnabled()) {
    background_color = cros_tokens::kCrosSysDisabledContainer;
  } else if (background_color_) {
    background_color = background_color_.value();
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
  if (enable_background_blur_) {
    blurred_background_->SetColor(background_color);
  } else {
    SetBackground(views::CreateRoundedRectBackground(
        background_color, gfx::RoundedCornersF(corner_radius)));
  }
}

views::PropertyEffects PillButton::UpdateStyleToIndicateDefaultStatus() {
  // Override the method defined in LabelButton to avoid style changes when the
  // `is_default_` flag is updated.
  return views::PropertyEffects::kNone;
}

void PillButton::SetText(std::u16string_view text) {
  std::u16string old_label_text(GetText());
  views::LabelButton::SetText(text);

  // This custom logic is necessary when the cached value for the tooltip is the
  // label's text.
  // Using our `UpdateTooltip()` function as-is would produce incorrect results
  // because the cache contains a value that did not originate from the parent
  // `LabelButton`.
  if (use_label_as_default_tooltip_ && old_label_text == GetTooltipText()) {
    SetTooltipText(std::u16string(GetText()));
  }
}

void PillButton::OnSetTooltipText(const std::u16string& tooltip_text) {
  views::LabelButton::OnSetTooltipText(tooltip_text);
  // We only update the `original_tooltip_text_` if the tooltip is not the
  // label's text.
  if (GetTooltipText() == GetText()) {
    return;
  }

  original_tooltip_text_ = GetTooltipText();
  UpdateTooltipText();
}

void PillButton::SetBackgroundColor(ui::ColorVariant background_color) {
  if (MaybeUpdateColorVariant(background_color_, background_color)) {
    UpdateBackgroundColor();
  }
}

void PillButton::SetButtonTextColor(ui::ColorVariant text_color) {
  if (MaybeUpdateColorVariant(text_color_, text_color)) {
    UpdateTextColor();
  }
}

void PillButton::SetIconColor(ui::ColorVariant icon_color) {
  if (MaybeUpdateColorVariant(icon_color_, icon_color)) {
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
  UpdateTooltipText();
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

  // Add padding around focus highlight only.
  views::FocusRing::Get(this)->SetPathGenerator(
      std::make_unique<views::RoundRectHighlightPathGenerator>(
          gfx::Insets(-kFocusRingPadding), height / 2.f + kFocusRingPadding));

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
  SetTextColor(views::Button::STATE_DISABLED, cros_tokens::kCrosSysDisabled);

  // If custom text color is set, use it to set text color.
  if (text_color_) {
    SetEnabledTextColors(text_color_);
  } else {
    // Otherwise, use default color ID to set text color.
    auto default_color_id = GetDefaultButtonTextIconColorId(type_);
    DCHECK(default_color_id);
    SetEnabledTextColors(default_color_id.value());
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
  if (icon_color_) {
    SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(*icon_, *icon_color_, kIconSize));
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

void PillButton::UpdateTooltipText() {
  const auto& tooltip = GetTooltipText();
  if (use_label_as_default_tooltip_ && tooltip.empty()) {
    SetTooltipText(std::u16string(GetText()));
  } else {
    // Only use the old value if we were using Label's Text as tooltip before.
    if (tooltip == GetText()) {
      SetTooltipText(original_tooltip_text_);
    }
  }
}

BEGIN_METADATA(PillButton)
END_METADATA

}  // namespace ash

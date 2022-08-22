// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/pill_button.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
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
constexpr int kPaddingReductionForIcon = 4;

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
  return type & PillButton::kIconLeading;
}

// Returns the button height according to the given type.
int GetButtonHeight(PillButton::Type type) {
  return (type & PillButton::kLarge) ? kPillButtonLargeHeight
                                     : kPillButtonHeight;
}

absl::optional<ui::ColorId> GetDefaultBackgroundColorId(PillButton::Type type) {
  absl::optional<ui::ColorId> color_id;

  const bool is_jellyroll_enabled = features::IsJellyrollEnabled();

  switch (type & kButtonColorVariant) {
    case PillButton::kDefault:
      color_id = is_jellyroll_enabled
                     ? static_cast<ui::ColorId>(cros_tokens::kCrosSysSysOnBase)
                     : static_cast<ui::ColorId>(
                           ash::kColorAshControlBackgroundColorInactive);
      break;
    case PillButton::kDefaultElevated:
      color_id = cros_tokens::kCrosSysSysBaseElevated;
      break;
    case PillButton::kPrimary:
      color_id = is_jellyroll_enabled
                     ? static_cast<ui::ColorId>(cros_tokens::kCrosSysPrimary)
                     : static_cast<ui::ColorId>(
                           ash::kColorAshControlBackgroundColorActive);
      break;
    case PillButton::kSecondary:
      color_id = cros_tokens::kCrosRefPrimary70;
      break;
    case PillButton::kAlert:
      color_id = is_jellyroll_enabled
                     ? static_cast<ui::ColorId>(cros_tokens::kCrosSysError)
                     : static_cast<ui::ColorId>(
                           ash::kColorAshControlBackgroundColorAlert);
      break;
    case PillButton::kAccent:
      color_id = ash::kColorAshControlBackgroundColorInactive;
      break;
    case PillButton::kFloating:
    case PillButton::kAccent | PillButton::kFloating:
      break;
    default:
      NOTREACHED() << "Invalid pill button type: " << type;
  }

  return color_id;
}

absl::optional<ui::ColorId> GetDefaultButtonTextIconColorId(
    PillButton::Type type) {
  absl::optional<ui::ColorId> color_id;

  const bool is_jellyroll_enabled = features::IsJellyrollEnabled();

  switch (type & kButtonColorVariant) {
    case PillButton::kDefault:
      color_id = is_jellyroll_enabled
                     ? static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface)
                     : static_cast<ui::ColorId>(ash::kColorAshButtonLabelColor);
      break;
    case PillButton::kDefaultElevated:
      color_id = cros_tokens::kCrosSysOnSurface;
      break;
    case PillButton::kPrimary:
      color_id =
          is_jellyroll_enabled
              ? static_cast<ui::ColorId>(cros_tokens::kCrosSysOnPrimary)
              : static_cast<ui::ColorId>(ash::kColorAshButtonLabelColorPrimary);
      break;
    case PillButton::kSecondary:
      color_id = cros_tokens::kCrosSysOnPrimaryContainer;
      break;
    case PillButton::kFloating:
      color_id = is_jellyroll_enabled
                     ? static_cast<ui::ColorId>(cros_tokens::kCrosSysPrimary)
                     : static_cast<ui::ColorId>(ash::kColorAshButtonLabelColor);
      break;
    case PillButton::kAlert:
      color_id =
          is_jellyroll_enabled
              ? static_cast<ui::ColorId>(cros_tokens::kCrosSysOnError)
              : static_cast<ui::ColorId>(ash::kColorAshButtonLabelColorPrimary);
      break;
    case PillButton::kAccent:
    case PillButton::kAccent | PillButton::kFloating:
      color_id = ash::kColorAshButtonLabelColorBlue;
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
                       bool use_light_colors,
                       bool rounded_highlight_path)
    : views::LabelButton(std::move(callback), text),
      type_(type),
      icon_(icon),
      use_light_colors_(use_light_colors),
      horizontal_spacing_(horizontal_spacing),
      rounded_highlight_path_(rounded_highlight_path) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  InitializeButtonLayout();
  label()->SetSubpixelRenderingEnabled(false);
  // TODO: Unify the font size, weight under ash/style as well.
  label()->SetFontList(views::Label::GetDefaultFontList().Derive(
      1, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  StyleUtil::SetUpInkDropForButton(
      this, gfx::Insets(),
      /*highlight_on_hover=*/false,
      /*highlight_on_focus=*/false,
      /*background_color=*/
      use_light_colors ? SK_ColorWHITE : gfx::kPlaceholderColor);
  views::FocusRing::Get(this)->SetColorId(
      (use_light_colors_ && !features::IsDarkLightModeEnabled())
          ? ui::kColorAshLightFocusRing
          : ui::kColorAshFocusRing);
  SetTooltipText(text);
}

PillButton::~PillButton() = default;

gfx::Size PillButton::CalculatePreferredSize() const {
  int button_width = label()->GetPreferredSize().width();

  if (IsIconPillButton(type_)) {
    // Add the padding on two sides.
    button_width += horizontal_spacing_ + GetHorizontalSpacingWithIcon();

    // Add the icon width and the spacing between the icon and the text.
    button_width += kIconSize + kIconPillButtonImageLabelSpacingDp;
  } else {
    button_width += 2 * horizontal_spacing_;
  }

  const int height = GetButtonHeight(type_);
  gfx::Size size(button_width, height);
  size.SetToMax(gfx::Size(kPillButtonMinimumWidth, height));
  return size;
}

int PillButton::GetHeightForWidth(int width) const {
  return GetButtonHeight(type_);
}

void PillButton::OnThemeChanged() {
  // If the button is not added to a widget, we don't have to update the color.
  if (!GetWidget())
    return;

  views::LabelButton::OnThemeChanged();

  const ui::ColorProvider* color_provider = GetColorProvider();
  auto default_text_icon_color_id = GetDefaultButtonTextIconColorId(type_);

  DCHECK(default_text_icon_color_id);

  SkColor enabled_icon_color = icon_color_.value_or(
      color_provider->GetColor(*default_text_icon_color_id));
  SkColor enabled_text_color = text_color_.value_or(
      color_provider->GetColor(*default_text_icon_color_id));
  if (background() && background_color_)
    background()->SetNativeControlColor(background_color_.value());

  // Override the colors to light mode if `use_light_colors_` is true when D/L
  // is not enabled.
  if (use_light_colors_ && !features::IsDarkLightModeEnabled()) {
    ScopedLightModeAsDefault scoped_light_mode_as_default;
    auto light_text_icon_color_id = GetDefaultButtonTextIconColorId(type_);
    DCHECK(light_text_icon_color_id);
    enabled_icon_color = icon_color_.value_or(
        color_provider->GetColor(*light_text_icon_color_id));
    enabled_text_color = text_color_.value_or(
        color_provider->GetColor(*light_text_icon_color_id));
    if (background() && background_color_)
      background()->SetNativeControlColor(background_color_.value());
  }

  if (IsIconPillButton(type_)) {
    DCHECK(icon_);
    SetImage(views::Button::STATE_NORMAL,
             gfx::CreateVectorIcon(*icon_, kIconSize, enabled_icon_color));
    SetImage(views::Button::STATE_DISABLED,
             gfx::CreateVectorIcon(
                 *icon_, kIconSize,
                 AshColorProvider::GetDisabledColor(enabled_icon_color)));
    SetImageLabelSpacing(kIconPillButtonImageLabelSpacingDp);
  }

  SetEnabledTextColors(enabled_text_color);
  SetTextColor(views::Button::STATE_DISABLED,
               AshColorProvider::GetDisabledColor(enabled_text_color));
}

void PillButton::SetBackgroundColor(const SkColor background_color) {
  if (background_color_ == background_color)
    return;

  background_color_ = background_color;
  DCHECK(background());
  background()->SetNativeControlColor(background_color_.value());
}

void PillButton::SetButtonTextColor(const SkColor text_color) {
  if (text_color_ == text_color)
    return;

  text_color_ = text_color;
  OnThemeChanged();
}

void PillButton::SetIconColor(const SkColor icon_color) {
  if (icon_color_ == icon_color)
    return;

  icon_color_ = icon_color;
  OnThemeChanged();
}

void PillButton::SetPillButtonType(Type type) {
  if (type_ == type)
    return;

  type_ = type;
  OnThemeChanged();
}

void PillButton::SetUseDefaultLabelFont() {
  label()->SetFontList(views::Label::GetDefaultFontList());
}

void PillButton::InitializeButtonLayout() {
  const int height = GetButtonHeight(type_);

  const int vertical_spacing =
      std::max((height - GetPreferredSize().height()) / 2, 0);
  const int left_padding = IsIconPillButton(type_)
                               ? GetHorizontalSpacingWithIcon()
                               : horizontal_spacing_;
  SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      vertical_spacing, left_padding, vertical_spacing, horizontal_spacing_)));

  if (rounded_highlight_path_) {
    if ((type_ & kButtonColorVariant) == kPrimary) {
      views::InstallRoundRectHighlightPathGenerator(
          this, gfx::Insets(-kFocusRingPadding),
          height / 2.f + kFocusRingPadding);
    } else {
      views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                    height / 2.f);
    }
  }

  if (!IsFloatingPillButton(type_)) {
    auto color_id = GetDefaultBackgroundColorId(type_);
    DCHECK(color_id);
    SetBackground(views::CreateThemedRoundedRectBackground(color_id.value(),
                                                           height / 2.f));
  }
  PreferredSizeChanged();
}

int PillButton::GetHorizontalSpacingWithIcon() const {
  return std::max(horizontal_spacing_ - kPaddingReductionForIcon, 0);
}

BEGIN_METADATA(PillButton, views::LabelButton)
END_METADATA

}  // namespace ash

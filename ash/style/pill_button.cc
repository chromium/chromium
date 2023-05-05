// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/pill_button.h"

#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/color_util.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "chromeos/constants/chromeos_features.h"
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

absl::optional<ui::ColorId> GetDefaultBackgroundColorId(PillButton::Type type) {
  absl::optional<ui::ColorId> color_id;

  const bool is_jellyroll_enabled = chromeos::features::IsJellyrollEnabled();

  switch (type & kButtonColorVariant) {
    case PillButton::kDefault:
      color_id =
          is_jellyroll_enabled
              ? static_cast<ui::ColorId>(cros_tokens::kCrosSysSystemOnBase)
              : static_cast<ui::ColorId>(
                    ash::kColorAshControlBackgroundColorInactive);
      break;
    case PillButton::kDefaultElevated:
      color_id = cros_tokens::kCrosSysSystemBaseElevated;
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
    default:
      NOTREACHED() << "Invalid and floating pill button type: " << type;
  }

  return color_id;
}

absl::optional<ui::ColorId> GetDefaultButtonTextIconColorId(
    PillButton::Type type) {
  absl::optional<ui::ColorId> color_id;

  const bool is_jellyroll_enabled = chromeos::features::IsJellyrollEnabled();

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
      color_id = cros_tokens::kCrosSysOnSecondaryContainer;
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
  views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);
  SetTooltipText(text);

  // Initialize image and icon spacing.
  SetImageLabelSpacing(kIconPillButtonImageLabelSpacingDp);

  enabled_changed_subscription_ = AddEnabledChangedCallback(base::BindRepeating(
      &PillButton::UpdateBackgroundColor, base::Unretained(this)));
}

PillButton::~PillButton() = default;

void PillButton::AddedToWidget() {
  // Only initialize the button after the button is added to a widget.
  Init();
}

gfx::Size PillButton::CalculatePreferredSize() const {
  int button_width = label()->GetPreferredSize().width();

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

int PillButton::GetHeightForWidth(int width) const {
  return GetButtonHeight(type_);
}

void PillButton::OnThemeChanged() {
  // If the button is not added to a widget, we don't have to update the color.
  if (!GetWidget())
    return;

  views::LabelButton::OnThemeChanged();
  UpdateTextColor();
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
  if (IsFloatingPillButton(type_))
    return;

  const int height = GetButtonHeight(type_);
  if (!GetEnabled()) {
    SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysDisabledContainer, height / 2.f));
    return;
  }

  // If custom color is set, use it to create a solid background.
  if (background_color_) {
    SetBackground(views::CreateRoundedRectBackground(background_color_.value(),
                                                     height / 2.f));
    return;
  }

  // Otherwise, use custom ID if set or default color ID to create a themed
  // background.
  auto default_color_id = GetDefaultBackgroundColorId(type_);
  DCHECK(default_color_id);
  SetBackground(views::CreateThemedRoundedRectBackground(
      background_color_id_.value_or(default_color_id.value()), height / 2.f));
}

void PillButton::SetBackgroundColor(const SkColor background_color) {
  if (background_color_ && background_color_.value() == background_color) {
    return;
  }

  background_color_ = background_color;
  background_color_id_ = absl::nullopt;
  UpdateBackgroundColor();
}

void PillButton::SetBackgroundColorId(ui::ColorId background_color_id) {
  if (background_color_id_ &&
      background_color_id_.value() == background_color_id) {
    return;
  }

  background_color_id_ = background_color_id;
  background_color_ = absl::nullopt;
  UpdateBackgroundColor();
}

void PillButton::SetButtonTextColor(const SkColor text_color) {
  if (text_color_ && text_color_.value() == text_color) {
    return;
  }

  text_color_ = text_color;
  text_color_id_ = absl::nullopt;
  UpdateTextColor();
}

void PillButton::SetButtonTextColorId(ui::ColorId text_color_id) {
  if (text_color_id_ && text_color_id_.value() == text_color_id) {
    return;
  }

  text_color_id_ = text_color_id;
  text_color_ = absl::nullopt;
  UpdateTextColor();
}

void PillButton::SetIconColor(const SkColor icon_color) {
  if (icon_color_ && icon_color_.value() == icon_color) {
    return;
  }

  icon_color_ = icon_color;
  icon_color_id_ = absl::nullopt;
  UpdateIconColor();
}

void PillButton::SetIconColorId(ui::ColorId icon_color_id) {
  if (icon_color_id_ && icon_color_id_.value() == icon_color_id) {
    return;
  }

  icon_color_id_ = icon_color_id;
  icon_color_ = absl::nullopt;
  UpdateIconColor();
}

void PillButton::SetPillButtonType(Type type) {
  if (type_ == type)
    return;

  type_ = type;
  Init();
}

void PillButton::SetUseDefaultLabelFont() {
  label()->SetFontList(views::Label::GetDefaultFontList());
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

  UpdateBackgroundColor();
  UpdateTextColor();
  UpdateIconColor();

  PreferredSizeChanged();
}

void PillButton::UpdateTextColor() {
  // Only update text color when the button is added to a widget.
  if (!GetWidget())
    return;

  // TODO(b:272787322): When LabelButton is able to use color ID, directly
  // use color ID for default text color.
  auto* color_provider = GetColorProvider();
  SetTextColor(views::Button::STATE_DISABLED,
               color_provider->GetColor(cros_tokens::kCrosSysDisabled));

  // If custom text color is set, use it to set text color.
  if (text_color_) {
    SetEnabledTextColors(text_color_.value());
    return;
  }

  // Otherwise, use custom color ID if set or default color ID to set text
  // color.
  auto default_color_id = GetDefaultButtonTextIconColorId(type_);
  DCHECK(default_color_id);
  SetEnabledTextColors(color_provider->GetColor(
      text_color_id_.value_or(default_color_id.value())));
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
    SetImage(views::Button::STATE_NORMAL,
             gfx::CreateVectorIcon(*icon_, kIconSize, icon_color_.value()));
    return;
  }

  // Otherwise, use custom color ID if set or default color ID to set icon
  // color.
  auto default_color_id = GetDefaultButtonTextIconColorId(type_);
  DCHECK(default_color_id);
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(
                    *icon_, icon_color_id_.value_or(default_color_id.value()),
                    kIconSize));
}

int PillButton::GetHorizontalSpacingWithIcon() const {
  return std::max(horizontal_spacing_ - padding_reduction_for_icon_, 0);
}

BEGIN_METADATA(PillButton, views::LabelButton)
END_METADATA

}  // namespace ash

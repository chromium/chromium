// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/pill_button.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/color_util.h"
#include "ash/style/style_util.h"
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
  return type & (PillButton::kIconLeading | PillButton::kIconFollowing);
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
  UpdateTextColor();
}

gfx::Insets PillButton::GetInsets() const {
  const int height = GetButtonHeight(type_);
  const int vertical_spacing =
      std::max((height - GetPreferredSize().height()) / 2, 0);
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

  if (background_color_) {
    SetBackground(views::CreateRoundedRectBackground(background_color_.value(),
                                                     height / 2.f));
    return;
  }

  auto background_color_id = GetDefaultBackgroundColorId(type_);
  DCHECK(background_color_id);
  SetBackground(views::CreateThemedRoundedRectBackground(
      background_color_id.value(), height / 2.f));
}

void PillButton::SetBackgroundColor(const SkColor background_color) {
  if (background_color_ == background_color)
    return;

  background_color_ = background_color;
  UpdateBackgroundColor();
}

void PillButton::SetButtonTextColor(const SkColor text_color) {
  if (text_color_ == text_color)
    return;

  text_color_ = text_color;
  UpdateTextColor();
}

void PillButton::SetIconColor(const SkColor icon_color) {
  if (icon_color_ == icon_color)
    return;

  icon_color_ = icon_color;
  UpdateIconColor();
}

void PillButton::SetPillButtonType(Type type) {
  if (type_ == type)
    return;

  type_ = type;

  if (GetWidget())
    Init();
}

void PillButton::SetUseDefaultLabelFont() {
  label()->SetFontList(views::Label::GetDefaultFontList());
}

void PillButton::Init() {
  DCHECK(GetWidget());

  if (type_ & kIconFollowing)
    SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  else
    SetHorizontalAlignment(gfx::ALIGN_CENTER);

  const int height = GetButtonHeight(type_);

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

  UpdateBackgroundColor();
  UpdateIconColor();
  UpdateTextColor();

  PreferredSizeChanged();
}

void PillButton::UpdateTextColor() {
  // Only update text color when the button is added to a widget.
  if (!GetWidget())
    return;

  // TODO(crbug.com/1383544): When LabelButton is able to use color ID, directly
  // use color ID for default text color.
  auto* color_provider = GetColorProvider();
  auto default_color_id = GetDefaultButtonTextIconColorId(type_);
  DCHECK(default_color_id);
  SetEnabledTextColors(
      text_color_.value_or(color_provider->GetColor(default_color_id.value())));
  SetTextColor(views::Button::STATE_DISABLED,
               color_provider->GetColor(cros_tokens::kCrosSysDisabled));
}

void PillButton::UpdateIconColor() {
  if (!IsIconPillButton(type_))
    return;

  DCHECK(icon_);
  if (icon_color_) {
    SetImage(views::Button::STATE_NORMAL,
             gfx::CreateVectorIcon(*icon_, kIconSize, icon_color_.value()));
  } else {
    auto default_color_id = GetDefaultButtonTextIconColorId(type_);
    DCHECK(default_color_id);
    SetImageModel(views::Button::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(
                      *icon_, default_color_id.value(), kIconSize));
  }
  SetImageModel(views::Button::STATE_DISABLED,
                ui::ImageModel::FromVectorIcon(
                    *icon_, cros_tokens::kCrosSysDisabled, kIconSize));
  SetImageLabelSpacing(kIconPillButtonImageLabelSpacingDp);
}

int PillButton::GetHorizontalSpacingWithIcon() const {
  return std::max(horizontal_spacing_ - kPaddingReductionForIcon, 0);
}

BEGIN_METADATA(PillButton, views::LabelButton)
END_METADATA

}  // namespace ash

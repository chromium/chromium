// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/button_style.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "base/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/rect_based_targeting_utils.h"

namespace ash {

namespace {

constexpr int kPillButtonHeight = 32;
constexpr int kPillButtonHorizontalSpacing = 16;
constexpr int kPillButtonMinimumWidth = 56;
constexpr int kIconSize = 20;
constexpr int kIconPillButtonImageLabelSpacingDp = 8;

constexpr int kSmallButtonSize = 16;
constexpr int kMediumButtonSize = 24;
constexpr int kLargeButtonSize = 32;

int GetCloseButtonSize(CloseButton::Type type) {
  switch (type) {
    case CloseButton::Type::kSmall:
      return kSmallButtonSize;
    case CloseButton::Type::kMedium:
      return kMediumButtonSize;
    case CloseButton::Type::kLarge:
      return kLargeButtonSize;
  }
}

SkColor GetCloseButtonBackgroundColor(bool use_light_colors) {
  auto* color_provider = AshColorProvider::Get();
  if (use_light_colors) {
    ScopedLightModeAsDefault scoped_light_mode_as_default;
    return color_provider->GetBaseLayerColor(
        AshColorProvider::BaseLayerType::kTransparent80);
  }
  return color_provider->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80);
}

// Returns true it is a floating type of PillButton, which is a type of
// PillButton without a background.
bool IsFloatingPillButton(PillButton::Type type) {
  return type == PillButton::Type::kIconlessFloating ||
         type == PillButton::Type::kIconlessAccentFloating;
}

SkColor GetPillButtonBackgroundColor(PillButton::Type type) {
  AshColorProvider::ControlsLayerType color_id =
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive;
  switch (type) {
    case PillButton::Type::kIcon:
    case PillButton::Type::kIconless:
    case PillButton::Type::kIconlessAccent:
      break;
    case PillButton::Type::kIconlessAlert:
      color_id =
          AshColorProvider::ControlsLayerType::kControlBackgroundColorAlert;
      break;
    case PillButton::Type::kIconlessProminent:
      color_id =
          AshColorProvider::ControlsLayerType::kControlBackgroundColorActive;
      break;
    case PillButton::Type::kIconlessFloating:
    case PillButton::Type::kIconlessAccentFloating:
      return SK_ColorTRANSPARENT;
  }
  return AshColorProvider::Get()->GetControlsLayerColor(color_id);
}

SkColor GetPillButtonTextColor(PillButton::Type type) {
  AshColorProvider::ContentLayerType color_id =
      AshColorProvider::ContentLayerType::kButtonLabelColor;
  switch (type) {
    case PillButton::Type::kIcon:
    case PillButton::Type::kIconless:
    case PillButton::Type::kIconlessProminent:
    case PillButton::Type::kIconlessFloating:
      break;
    case PillButton::Type::kIconlessAlert:
      color_id = AshColorProvider::ContentLayerType::kButtonLabelColorPrimary;
      break;
    case PillButton::Type::kIconlessAccent:
    case PillButton::Type::kIconlessAccentFloating:
      color_id = AshColorProvider::ContentLayerType::kButtonLabelColorBlue;
      break;
  }
  return AshColorProvider::Get()->GetContentLayerColor(color_id);
}

int GetPillButtonWidth(bool has_icon) {
  int button_width = 2 * kPillButtonHorizontalSpacing;
  if (has_icon)
    button_width += (kIconSize + kIconPillButtonImageLabelSpacingDp);
  return button_width;
}

SkColor GetBackgroundColorForInkDrop(bool use_light_colors) {
  return use_light_colors ? SK_ColorWHITE : SK_ColorBLACK;
}

}  // namespace

CloseButton::CloseButton(PressedCallback callback,
                         CloseButton::Type type,
                         bool use_light_colors)
    : ImageButton(std::move(callback)),
      type_(type),
      use_light_colors_(use_light_colors) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
  StyleUtil::SetUpInkDropForButton(
      this, gfx::Insets(),
      /*highlight_on_hover=*/true,
      /*highlight_on_focus=*/false,
      /*background_color=*/GetBackgroundColorForInkDrop(use_light_colors_));

  // Add a rounded rect background. The rounding will be half the button size so
  // it is a circle.
  SetBackground(views::CreateRoundedRectBackground(
      GetCloseButtonBackgroundColor(use_light_colors_),
      GetCloseButtonSize(type_) / 2));

  SetFocusPainter(nullptr);
  SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  views::InstallCircleHighlightPathGenerator(this);
}

CloseButton::~CloseButton() = default;

bool CloseButton::DoesIntersectScreenRect(const gfx::Rect& screen_rect) const {
  gfx::Point origin = screen_rect.origin();
  View::ConvertPointFromScreen(this, &origin);
  return DoesIntersectRect(this, gfx::Rect(origin, screen_rect.size()));
}

void CloseButton::OnThemeChanged() {
  views::ImageButton::OnThemeChanged();
  background()->SetNativeControlColor(
      GetCloseButtonBackgroundColor(use_light_colors_));
  auto* color_provider = AshColorProvider::Get();
  SkColor enabled_icon_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
  if (use_light_colors_) {
    ScopedLightModeAsDefault scoped_light_mode_as_default;
    enabled_icon_color = color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kButtonIconColor);
  }
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(kCloseButtonIcon, enabled_icon_color));

  // TODO(minch): Add background blur as per spec. Background blur is quite
  // heavy, and we may have many close buttons showing at a time. They'll be
  // added separately so its easier to monitor performance.

  StyleUtil::ConfigureInkDropAttributes(this, StyleUtil::kBaseColor |
                                                  StyleUtil::kInkDropOpacity |
                                                  StyleUtil::kHighlightOpacity);
}

gfx::Size CloseButton::CalculatePreferredSize() const {
  const int size = GetCloseButtonSize(type_);
  return gfx::Size(size, size);
}

bool CloseButton::DoesIntersectRect(const views::View* target,
                                    const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);
  gfx::Rect button_bounds = target->GetLocalBounds();
  const int button_size = GetCloseButtonSize(type_);
  // Only increase the hittest area for touch events (which have a non-empty
  // bounding box), not for mouse event.
  if (!views::UsePointBasedTargeting(rect))
    button_bounds.Inset(gfx::Insets(-button_size / 2, -button_size / 2));
  return button_bounds.Intersects(rect);
}

BEGIN_METADATA(CloseButton, views::ImageButton)
END_METADATA

PillButton::PillButton(PressedCallback callback,
                       const std::u16string& text,
                       PillButton::Type type,
                       const gfx::VectorIcon* icon,
                       bool use_light_colors,
                       bool rounded_highlight_path)
    : views::LabelButton(std::move(callback), text),
      type_(type),
      icon_(icon),
      use_light_colors_(use_light_colors) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  const int vertical_spacing =
      std::max(kPillButtonHeight - GetPreferredSize().height() / 2, 0);
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(vertical_spacing, kPillButtonHorizontalSpacing)));
  label()->SetSubpixelRenderingEnabled(false);
  // TODO: Unify the font size, weight under ash/style as well.
  label()->SetFontList(views::Label::GetDefaultFontList().Derive(
      1, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  StyleUtil::SetUpInkDropForButton(
      this, gfx::Insets(),
      /*highlight_on_hover=*/false,
      /*highlight_on_focus=*/false,
      /*background_color=*/GetBackgroundColorForInkDrop(use_light_colors));
  if (rounded_highlight_path) {
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  kPillButtonHeight / 2.f);
  }
  if (!IsFloatingPillButton(type_)) {
    SetBackground(views::CreateRoundedRectBackground(
        GetPillButtonBackgroundColor(type), kPillButtonHeight / 2.f));
  }
}

PillButton::~PillButton() = default;

gfx::Size PillButton::CalculatePreferredSize() const {
  gfx::Size size(label()->GetPreferredSize().width() +
                     GetPillButtonWidth(type_ == PillButton::Type::kIcon),
                 kPillButtonHeight);
  size.SetToMax(gfx::Size(kPillButtonMinimumWidth, kPillButtonHeight));
  return size;
}

int PillButton::GetHeightForWidth(int width) const {
  return kPillButtonHeight;
}

void PillButton::OnThemeChanged() {
  views::LabelButton::OnThemeChanged();

  auto* color_provider = AshColorProvider::Get();

  SkColor enabled_icon_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
  SkColor enabled_text_color = GetPillButtonTextColor(type_);
  views::FocusRing::Get(this)->SetColor(color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
  if (!IsFloatingPillButton(type_))
    background()->SetNativeControlColor(GetPillButtonBackgroundColor(type_));

  // Override the colors to light mode if `use_light_colors_` is true when D/L
  // is not enabled.
  if (use_light_colors_ && !features::IsDarkLightModeEnabled()) {
    ScopedLightModeAsDefault scoped_light_mode_as_default;
    enabled_icon_color = color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kButtonIconColor);
    enabled_text_color = GetPillButtonTextColor(type_);
    views::FocusRing::Get(this)->SetColor(color_provider->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kFocusRingColor));
    if (!IsFloatingPillButton(type_))
      background()->SetNativeControlColor(GetPillButtonBackgroundColor(type_));
  }

  if (type_ == PillButton::Type::kIcon) {
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

BEGIN_METADATA(PillButton, views::LabelButton)
END_METADATA

}  // namespace ash

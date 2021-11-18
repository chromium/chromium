// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/close_button.h"

#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
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

constexpr int kSmallButtonSize = 16;
constexpr int kMediumButtonSize = 20;
constexpr int kLargeButtonSize = 32;
constexpr int kSmallIconSize = 8;
constexpr int kMediumIconSize = 16;
constexpr int kLargeIconSize = 24;

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

int GetIconSize(CloseButton::Type type) {
  switch (type) {
    case CloseButton::Type::kSmall:
      return kSmallIconSize;
    case CloseButton::Type::kMedium:
      return kMediumIconSize;
    case CloseButton::Type::kLarge:
      return kLargeIconSize;
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
      /*background_color=*/
      use_light_colors ? SK_ColorBLACK : gfx::kPlaceholderColor);

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
           gfx::CreateVectorIcon(type_ == CloseButton::Type::kSmall
                                     ? kSmallCloseButtonIcon
                                     : kMediumOrLargeCloseButtonIcon,
                                 GetIconSize(type_), enabled_icon_color));

  // TODO(minch): Add background blur as per spec. Background blur is quite
  // heavy, and we may have many close buttons showing at a time. They'll be
  // added separately so its easier to monitor performance.

  StyleUtil::ConfigureInkDropAttributes(this, StyleUtil::kBaseColor);
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

}  // namespace ash

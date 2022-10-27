// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_color_provider_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/color_util.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"

namespace ash {

namespace {
// Opacity of the light/dark inkdrop.
constexpr float kLightInkDropOpacity = 0.08f;
constexpr float kDarkInkDropOpacity = 0.12f;

// Helper to check if tablet mode is enabled.
bool IsTabletModeEnabled() {
  return Shell::Get()->tablet_mode_controller() &&
         Shell::Get()->tablet_mode_controller()->InTabletMode();
}

bool IsDarkModeEnabled() {
  // May be null in unit tests.
  if (!Shell::HasInstance())
    return true;
  return DarkLightModeController::Get()->IsDarkModeEnabled();
}

}  // namespace

AppListColorProviderImpl::AppListColorProviderImpl()
    : is_background_blur_enabled_(features::IsBackgroundBlurEnabled()) {}

AppListColorProviderImpl::~AppListColorProviderImpl() = default;

SkColor AppListColorProviderImpl::GetSearchBoxBackgroundColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  const ui::ColorProvider* color_provider = app_list_widget->GetColorProvider();
  if (IsTabletModeEnabled()) {
    return color_provider->GetColor(is_background_blur_enabled_
                                        ? kColorAshShieldAndBase80
                                        : kColorAshShieldAndBase95);
  }
  return color_provider->GetColor(kColorAshControlBackgroundColorInactive);
}

SkColor AppListColorProviderImpl::GetSearchBoxCardBackgroundColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  return app_list_widget->GetColorProvider()->GetColor(
      is_background_blur_enabled_ ? kColorAshShieldAndBase80
                                  : kColorAshShieldAndBase95);
}

SkColor AppListColorProviderImpl::GetSearchBoxTextColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  return app_list_widget->GetColorProvider()->GetColor(
      cros_tokens::kTextColorPrimary);
}

SkColor AppListColorProviderImpl::GetSearchBoxSecondaryTextColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  return app_list_widget->GetColorProvider()->GetColor(
      cros_tokens::kTextColorSecondary);
}

SkColor AppListColorProviderImpl::GetSearchBoxSuggestionTextColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  return app_list_widget->GetColorProvider()->GetColor(
      kColorAshTextColorSuggestion);
}

SkColor AppListColorProviderImpl::GetAppListItemTextColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  return app_list_widget->GetColorProvider()->GetColor(
      cros_tokens::kTextColorPrimary);
}

SkColor AppListColorProviderImpl::GetPageSwitcherButtonColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  return app_list_widget->GetColorProvider()->GetColor(
      kColorAshButtonIconColor);
}

SkColor AppListColorProviderImpl::GetSearchBoxIconColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  return app_list_widget->GetColorProvider()->GetColor(
      kColorAshButtonIconColor);
}

SkColor AppListColorProviderImpl::GetFolderBackgroundColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  return app_list_widget->GetColorProvider()->GetColor(
      kColorAshShieldAndBase80);
}

SkColor AppListColorProviderImpl::GetFolderTitleTextColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  return app_list_widget->GetColorProvider()->GetColor(
      cros_tokens::kTextColorPrimary);
}

SkColor AppListColorProviderImpl::GetFolderHintTextColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  return app_list_widget->GetColorProvider()->GetColor(
      cros_tokens::kTextColorSecondary);
}

SkColor AppListColorProviderImpl::GetFolderNameBorderColor(
    bool active,
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  if (!active)
    return SK_ColorTRANSPARENT;

  return app_list_widget->GetColorProvider()->GetColor(ui::kColorAshFocusRing);
}

SkColor AppListColorProviderImpl::GetFolderNameSelectionColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  return app_list_widget->GetColorProvider()->GetColor(kColorAshFocusAuraColor);
}

SkColor AppListColorProviderImpl::GetFolderNotificationBadgeColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  return app_list_widget->GetColorProvider()->GetColor(
      cros_tokens::kIconColorBlue);
}

SkColor AppListColorProviderImpl::GetContentsBackgroundColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  return app_list_widget->GetColorProvider()->GetColor(
      kColorAshControlBackgroundColorInactive);
}

SkColor AppListColorProviderImpl::GetGridBackgroundCardActiveColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  SkColor background_color =
      GetGridBackgroundCardInactiveColor(app_list_widget);
  if (background_color == gfx::kPlaceholderColor) {
    background_color = ColorUtil::GetBackgroundThemedColor(
        app_list_widget->GetColorProvider()->GetColor(
            kColorAshShieldAndBaseOpaque),
        IsDarkModeEnabled());
  }

  const float opacity = color_utils::IsDark(background_color)
                            ? kLightInkDropOpacity
                            : kDarkInkDropOpacity;

  return SkColorSetA(background_color,
                     SkColorGetA(background_color) + 255 * (1.0f + opacity));
}

SkColor AppListColorProviderImpl::GetGridBackgroundCardInactiveColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  return app_list_widget->GetColorProvider()->GetColor(
      kColorAshControlBackgroundColorInactive);
}

SkColor AppListColorProviderImpl::GetFocusRingColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  return app_list_widget->GetColorProvider()->GetColor(ui::kColorAshFocusRing);
}

SkColor AppListColorProviderImpl::GetInkDropBaseColor(
    const views::Widget* app_list_widget,
    SkColor bg_color) const {
  DCHECK(app_list_widget);

  return app_list_widget->GetColorProvider()->GetColor(
      kColorAshInkDropOpaqueColor);
}

float AppListColorProviderImpl::GetInkDropOpacity(
    const views::Widget* app_list_widget,
    SkColor bg_color) const {
  DCHECK(app_list_widget);

  if (bg_color == gfx::kPlaceholderColor) {
    bg_color = ColorUtil::GetBackgroundThemedColor(
        app_list_widget->GetColorProvider()->GetColor(
            kColorAshShieldAndBaseOpaque),
        IsDarkModeEnabled());
  }

  return color_utils::IsDark(bg_color) ? kLightInkDropOpacity
                                       : kDarkInkDropOpacity;
}

SkColor AppListColorProviderImpl::GetSearchResultViewHighlightColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  return app_list_widget->GetColorProvider()->GetColor(
      kColorAshHighlightColorHover);
}

SkColor AppListColorProviderImpl::GetTextColorURL(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);
  return app_list_widget->GetColorProvider()->GetColor(kColorAshTextColorURL);
}

}  // namespace ash

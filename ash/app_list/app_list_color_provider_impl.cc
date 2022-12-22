// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_color_provider_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/color_util.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"

namespace ash {

namespace {
// Opacity of the light/dark inkdrop.
constexpr float kLightInkDropOpacity = 0.08f;
constexpr float kDarkInkDropOpacity = 0.12f;

bool IsDarkModeEnabled() {
  // May be null in unit tests.
  if (!Shell::HasInstance())
    return true;
  return DarkLightModeController::Get()->IsDarkModeEnabled();
}

}  // namespace

AppListColorProviderImpl::AppListColorProviderImpl() = default;

AppListColorProviderImpl::~AppListColorProviderImpl() = default;

SkColor AppListColorProviderImpl::GetPageSwitcherButtonColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  return app_list_widget->GetColorProvider()->GetColor(
      kColorAshButtonIconColor);
}

SkColor AppListColorProviderImpl::GetFolderNotificationBadgeColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  return app_list_widget->GetColorProvider()->GetColor(
      cros_tokens::kIconColorBlue);
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

}  // namespace ash

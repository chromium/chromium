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
constexpr float kDarkInkDropOpacity = 0.06f;

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
    : is_dark_light_mode_enabled_(features::IsDarkLightModeEnabled()),
      is_productivity_launcher_enabled_(
          features::IsProductivityLauncherEnabled()),
      is_background_blur_enabled_(features::IsBackgroundBlurEnabled()) {}

AppListColorProviderImpl::~AppListColorProviderImpl() = default;

SkColor AppListColorProviderImpl::GetSearchBoxBackgroundColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  const ui::ColorProvider* color_provider = app_list_widget->GetColorProvider();
  if (ShouldUseDarkLightColors()) {
    if (IsTabletModeEnabled()) {
      return color_provider->GetColor(is_background_blur_enabled_
                                          ? kColorAshShieldAndBase80
                                          : kColorAshShieldAndBase95);
    } else {
      return color_provider->GetColor(kColorAshControlBackgroundColorInactive);
    }
  }
  return SK_ColorWHITE;  // default_color
}

SkColor AppListColorProviderImpl::GetSearchBoxCardBackgroundColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  if (ShouldUseDarkLightColors()) {
    return app_list_widget->GetColorProvider()->GetColor(
        is_background_blur_enabled_ ? kColorAshShieldAndBase80
                                    : kColorAshShieldAndBase95);
  }
  return SK_ColorWHITE;  // default_color
}

SkColor AppListColorProviderImpl::GetSearchBoxTextColor(
    SkColor default_color,
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  if (ShouldUseDarkLightColors()) {
    return app_list_widget->GetColorProvider()->GetColor(
        cros_tokens::kTextColorPrimary);
  }
  return default_color;
}

SkColor AppListColorProviderImpl::GetSearchBoxSecondaryTextColor(
    SkColor default_color,
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  if (ShouldUseDarkLightColors()) {
    return app_list_widget->GetColorProvider()->GetColor(
        cros_tokens::kTextColorSecondary);
  }
  return default_color;
}

SkColor AppListColorProviderImpl::GetSearchBoxSuggestionTextColor(
    SkColor default_color,
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  if (ShouldUseDarkLightColors()) {
    return app_list_widget->GetColorProvider()->GetColor(
        kColorAshTextColorSuggestion);
  }
  return default_color;
}

SkColor AppListColorProviderImpl::GetAppListItemTextColor(
    bool is_in_folder,
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  if (ShouldUseDarkLightColors()) {
    return app_list_widget->GetColorProvider()->GetColor(
        cros_tokens::kTextColorPrimary);
  }
  return is_in_folder ? SK_ColorBLACK : SK_ColorWHITE;
}

SkColor AppListColorProviderImpl::GetPageSwitcherButtonColor(
    bool is_root_app_grid_page_switcher,
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  if (ShouldUseDarkLightColors()) {
    return app_list_widget->GetColorProvider()->GetColor(
        kColorAshButtonIconColor);
  }
  // default_color
  return is_root_app_grid_page_switcher ? SkColorSetARGB(255, 232, 234, 237)
                                        : SkColorSetA(SK_ColorBLACK, 138);
}

SkColor AppListColorProviderImpl::GetSearchBoxIconColor(
    SkColor default_color,
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  if (ShouldUseDarkLightColors()) {
    return app_list_widget->GetColorProvider()->GetColor(
        kColorAshButtonIconColor);
  }
  return default_color;
}

SkColor AppListColorProviderImpl::GetFolderBackgroundColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  if (ShouldUseDarkLightColors()) {
    return app_list_widget->GetColorProvider()->GetColor(
        kColorAshShieldAndBase80);
  }
  return SK_ColorWHITE;
}

SkColor AppListColorProviderImpl::GetFolderTitleTextColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  if (ShouldUseDarkLightColors()) {
    return app_list_widget->GetColorProvider()->GetColor(
        cros_tokens::kTextColorPrimary);
  }
  return gfx::kGoogleGrey700;
}

SkColor AppListColorProviderImpl::GetFolderHintTextColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  if (ShouldUseDarkLightColors()) {
    return app_list_widget->GetColorProvider()->GetColor(
        cros_tokens::kTextColorSecondary);
  }
  return gfx::kGoogleGrey600;
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

  if (ShouldUseDarkLightColors()) {
    return app_list_widget->GetColorProvider()->GetColor(
        kColorAshControlBackgroundColorInactive);
  }
  return SkColorSetRGB(0xF2, 0xF2, 0xF2);  // default_color
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

ui::ColorId AppListColorProviderImpl::GetSeparatorColorId() const {
  if (ShouldUseDarkLightColors()) {
    return ui::kColorAshAppListSeparatorLight;
  }
  return ui::kColorAshAppListSeparator;  // default_color
}

SkColor AppListColorProviderImpl::GetFocusRingColor(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  if (ShouldUseDarkLightColors()) {
    return app_list_widget->GetColorProvider()->GetColor(
        ui::kColorAshFocusRing);
  }
  return gfx::kGoogleBlue600;  // default_color
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

  // Use highlight colors when Dark Light mode is enabled.
  if (ShouldUseDarkLightColors()) {
    return app_list_widget->GetColorProvider()->GetColor(
        kColorAshHighlightColorHover);
  }
  // Use inkdrop colors by default.
  return SkColorSetA(
      GetInkDropBaseColor(app_list_widget,
                          GetSearchBoxBackgroundColor(app_list_widget)),
      GetInkDropOpacity(app_list_widget,
                        GetSearchBoxBackgroundColor(app_list_widget)) *
          255);
}

SkColor AppListColorProviderImpl::GetTextColorURL(
    const views::Widget* app_list_widget) const {
  DCHECK(app_list_widget);

  // Use highlight colors when Dark Light mode is enabled.
  if (ShouldUseDarkLightColors()) {
    return app_list_widget->GetColorProvider()->GetColor(kColorAshTextColorURL);
  }
  return gfx::kGoogleBlue600;
}

bool AppListColorProviderImpl::ShouldUseDarkLightColors() const {
  return is_dark_light_mode_enabled_ || is_productivity_launcher_enabled_;
}

}  // namespace ash

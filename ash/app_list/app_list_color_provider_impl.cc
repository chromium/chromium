// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_color_provider_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/default_colors.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"

namespace ash {

namespace {

// Helper to check if tablet mode is enabled.
bool IsTabletModeEnabled() {
  return Shell::Get()->tablet_mode_controller() &&
         Shell::Get()->tablet_mode_controller()->InTabletMode();
}

}  // namespace

AppListColorProviderImpl::AppListColorProviderImpl()
    : ash_color_provider_(AshColorProvider::Get()),
      is_dark_light_mode_enabled_(features::IsDarkLightModeEnabled()),
      is_productivity_launcher_enabled_(
          features::IsProductivityLauncherEnabled()),
      is_background_blur_enabled_(features::IsBackgroundBlurEnabled()) {}

AppListColorProviderImpl::~AppListColorProviderImpl() = default;

SkColor AppListColorProviderImpl::GetExpandArrowIconBaseColor() const {
  if (ShouldUseDarkLightColors()) {
    return ash_color_provider_->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kButtonIconColor);
  }
  return SK_ColorWHITE;  // default_color
}

SkColor AppListColorProviderImpl::GetExpandArrowIconBackgroundColor() const {
  if (ShouldUseDarkLightColors()) {
    return ash_color_provider_->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  }
  return SkColorSetARGB(0xF, 0xFF, 0xFF, 0xFF);  // default_color
}

SkColor AppListColorProviderImpl::GetAppListBackgroundColor(
    bool is_tablet_mode,
    SkColor default_color) const {
  if (ShouldUseDarkLightColors()) {
    return ash_color_provider_->GetShieldLayerColor(
        is_tablet_mode ? AshColorProvider::ShieldLayerType::kShield40
                       : AshColorProvider::ShieldLayerType::kShield80);
  }
  return default_color;
}

SkColor AppListColorProviderImpl::GetSearchBoxBackgroundColor() const {
  if (ShouldUseDarkLightColors()) {
    if (IsTabletModeEnabled()) {
      return ash_color_provider_->GetBaseLayerColor(
          is_background_blur_enabled_
              ? AshColorProvider::BaseLayerType::kTransparent80
              : AshColorProvider::BaseLayerType::kTransparent95);
    } else {
      return ash_color_provider_->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
    }
  }
  return SK_ColorWHITE;  // default_color
}

SkColor AppListColorProviderImpl::GetSearchBoxCardBackgroundColor() const {
  if (ShouldUseDarkLightColors()) {
    return ash_color_provider_->GetBaseLayerColor(
        is_background_blur_enabled_
            ? AshColorProvider::BaseLayerType::kTransparent80
            : AshColorProvider::BaseLayerType::kTransparent95);
  }
  return SK_ColorWHITE;  // default_color
}

SkColor AppListColorProviderImpl::GetSearchBoxTextColor(
    SkColor default_color) const {
  if (ShouldUseDarkLightColors()) {
    return ash_color_provider_->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary);
  }
  return default_color;
}

SkColor AppListColorProviderImpl::GetSearchBoxSecondaryTextColor(
    SkColor default_color) const {
  if (ShouldUseDarkLightColors()) {
    return ash_color_provider_->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorSecondary);
  }
  return default_color;
}

SkColor AppListColorProviderImpl::GetSuggestionChipBackgroundColor() const {
  if (ShouldUseDarkLightColors()) {
    if (IsTabletModeEnabled()) {
      return ash_color_provider_->GetBaseLayerColor(
          AshColorProvider::BaseLayerType::kTransparent80);
    } else {
      return ash_color_provider_->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
    }
  }
  return SkColorSetA(gfx::kGoogleGrey100, 0x14);  // default_color
}

SkColor AppListColorProviderImpl::GetSuggestionChipTextColor() const {
  if (ShouldUseDarkLightColors()) {
    return ash_color_provider_->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary);
  }
  return gfx::kGoogleGrey100;  // default_color
}

SkColor AppListColorProviderImpl::GetAppListItemTextColor(
    bool is_in_folder) const {
  if (ShouldUseDarkLightColors()) {
    return ash_color_provider_->GetContentLayerColor(
        ColorProvider::ContentLayerType::kTextColorPrimary);
  }
  return is_in_folder ? SK_ColorBLACK : SK_ColorWHITE;
}

SkColor AppListColorProviderImpl::GetPageSwitcherButtonColor(
    bool is_root_app_grid_page_switcher) const {
  if (ShouldUseDarkLightColors()) {
    return ash_color_provider_->GetContentLayerColor(
        ColorProvider::ContentLayerType::kButtonIconColor);
  }
  // default_color
  return is_root_app_grid_page_switcher ? SkColorSetARGB(255, 232, 234, 237)
                                        : SkColorSetA(SK_ColorBLACK, 138);
}

SkColor AppListColorProviderImpl::GetSearchBoxIconColor(
    SkColor default_color) const {
  if (ShouldUseDarkLightColors()) {
    return ash_color_provider_->GetContentLayerColor(
        ColorProvider::ContentLayerType::kButtonIconColor);
  }
  return default_color;
}

SkColor AppListColorProviderImpl::GetFolderBackgroundColor() const {
  if (ShouldUseDarkLightColors()) {
    return ash_color_provider_->GetBaseLayerColor(
        ColorProvider::BaseLayerType::kTransparent80);
  }
  return SK_ColorWHITE;
}

SkColor AppListColorProviderImpl::GetFolderBubbleColor() const {
  if (ShouldUseDarkLightColors()) {
    return ash_color_provider_->GetControlsLayerColor(
        ColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  }
  return SkColorSetA(gfx::kGoogleGrey100, 0x7A);
}

SkColor AppListColorProviderImpl::GetFolderTitleTextColor() const {
  if (ShouldUseDarkLightColors()) {
    return ash_color_provider_->GetContentLayerColor(
        ColorProvider::ContentLayerType::kTextColorPrimary);
  }
  return gfx::kGoogleGrey700;
}

SkColor AppListColorProviderImpl::GetFolderHintTextColor() const {
  if (ShouldUseDarkLightColors()) {
    return ash_color_provider_->GetContentLayerColor(
        ColorProvider::ContentLayerType::kTextColorSecondary);
  }
  return gfx::kGoogleGrey600;
}

SkColor AppListColorProviderImpl::GetFolderNameBorderColor(bool active) const {
  if (!active)
    return SK_ColorTRANSPARENT;

  return ash_color_provider_->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor);
}

SkColor AppListColorProviderImpl::GetFolderNameSelectionColor() const {
  return ash_color_provider_->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusAuraColor);
}

SkColor AppListColorProviderImpl::GetContentsBackgroundColor() const {
  if (ShouldUseDarkLightColors()) {
    return ash_color_provider_->GetControlsLayerColor(
        ColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  }
  return SkColorSetRGB(0xF2, 0xF2, 0xF2);  // default_color
}

SkColor AppListColorProviderImpl::GetGridBackgroundCardActiveColor() const {
  const SkColor background_color = GetGridBackgroundCardInactiveColor();
  return SkColorSetA(
      background_color,
      SkColorGetA(background_color) +
          255 * (1.0f + AshColorProvider::Get()
                            ->GetInkDropBaseColorAndOpacity(background_color)
                            .second));
}

SkColor AppListColorProviderImpl::GetGridBackgroundCardInactiveColor() const {
  return AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
}

SkColor AppListColorProviderImpl::GetSeparatorColor() const {
  if (ShouldUseDarkLightColors()) {
    return ash_color_provider_->GetContentLayerColor(
        ColorProvider::ContentLayerType::kSeparatorColor);
  }
  return SkColorSetA(gfx::kGoogleGrey900, 0x24);  // default_color
}

SkColor AppListColorProviderImpl::GetFocusRingColor() const {
  if (ShouldUseDarkLightColors()) {
    return ash_color_provider_->GetControlsLayerColor(
        ColorProvider::ControlsLayerType::kFocusRingColor);
  }
  return gfx::kGoogleBlue600;  // default_color
}

SkColor AppListColorProviderImpl::GetInkDropBaseColor(SkColor bg_color) const {
  return ash_color_provider_->GetInkDropBaseColorAndOpacity(bg_color).first;
}

float AppListColorProviderImpl::GetInkDropOpacity(SkColor bg_color) const {
  return ash_color_provider_->GetInkDropBaseColorAndOpacity(bg_color).second;
}

SkColor AppListColorProviderImpl::GetInvertedInkDropBaseColor(
    SkColor bg_color) const {
  return ash_color_provider_->GetInvertedInkDropBaseColorAndOpacity(bg_color)
      .first;
}

float AppListColorProviderImpl::GetInvertedInkDropOpacity(
    SkColor bg_color) const {
  return ash_color_provider_->GetInvertedInkDropBaseColorAndOpacity(bg_color)
      .second;
}

SkColor AppListColorProviderImpl::GetSearchResultViewHighlightColor() const {
  // Use highlight colors when Dark Light mode is enabled.
  if (ShouldUseDarkLightColors()) {
    return ash_color_provider_->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kHighlightColorHover);
  }
  // Use inkdrop colors by default.
  return SkColorSetA(GetInkDropBaseColor(GetSearchBoxBackgroundColor()),
                     GetInkDropOpacity(GetSearchBoxBackgroundColor()) * 255);
}

SkColor AppListColorProviderImpl::GetTextColorURL() const {
  // Use highlight colors when Dark Light mode is enabled.
  if (ShouldUseDarkLightColors()) {
    return AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorURL);
  }
  return gfx::kGoogleBlue600;
}

bool AppListColorProviderImpl::ShouldUseDarkLightColors() const {
  return is_dark_light_mode_enabled_ || is_productivity_launcher_enabled_;
}

}  // namespace ash

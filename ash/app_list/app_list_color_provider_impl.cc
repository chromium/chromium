// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_color_provider_impl.h"

#include "ash/public/cpp/ash_features.h"
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
    : ash_color_provider_(AshColorProvider::Get()) {}

AppListColorProviderImpl::~AppListColorProviderImpl() = default;

SkColor AppListColorProviderImpl::GetExpandArrowIconBaseColor() const {
  return DeprecatedGetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor,
      /*default_color*/ SK_ColorWHITE);
}

SkColor AppListColorProviderImpl::GetExpandArrowIconBackgroundColor() const {
  return DeprecatedGetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive,
      /*default_color*/ SkColorSetARGB(0xF, 0xFF, 0xFF, 0xFF));
}

SkColor AppListColorProviderImpl::GetAppListBackgroundColor(
    bool is_tablet_mode) const {
  return DeprecatedGetShieldLayerColor(
      is_tablet_mode ? AshColorProvider::ShieldLayerType::kShield40
                     : AshColorProvider::ShieldLayerType::kShield80,
      /*default_color*/ gfx::kGoogleGrey900);
}

SkColor AppListColorProviderImpl::GetSearchBoxBackgroundColor() const {
  if (IsTabletModeEnabled()) {
    return DeprecatedGetBaseLayerColor(
        AshColorProvider::BaseLayerType::kTransparent80,
        /*default_color*/ SK_ColorWHITE);
  }

  return DeprecatedGetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive,
      SK_ColorWHITE);
}

SkColor AppListColorProviderImpl::GetSearchBoxCardBackgroundColor() const {
  return DeprecatedGetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80,
      /*default_color*/ SK_ColorWHITE);
}

SkColor AppListColorProviderImpl::GetSearchBoxTextColor(
    SkColor default_color) const {
  return DeprecatedGetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary, default_color);
}

SkColor AppListColorProviderImpl::GetSearchBoxSecondaryTextColor(
    SkColor default_color) const {
  return DeprecatedGetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary, default_color);
}

SkColor AppListColorProviderImpl::GetSuggestionChipBackgroundColor() const {
  if (IsTabletModeEnabled()) {
    return DeprecatedGetBaseLayerColor(
        AshColorProvider::BaseLayerType::kTransparent80,
        /*default_color*/ SkColorSetA(gfx::kGoogleGrey100, 0x14));
  }

  return DeprecatedGetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive,
      /*default_color*/ SkColorSetA(gfx::kGoogleGrey100, 0x14));
}

SkColor AppListColorProviderImpl::GetSuggestionChipTextColor() const {
  return DeprecatedGetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary,
      /*default_color*/ gfx::kGoogleGrey100);
}

SkColor AppListColorProviderImpl::GetAppListItemTextColor(
    bool is_in_folder) const {
  if (is_in_folder && !features::IsDarkLightModeEnabled())
    return SK_ColorBLACK;
  return DeprecatedGetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary,
      /*default_color*/ SK_ColorWHITE);
}

SkColor AppListColorProviderImpl::GetPageSwitcherButtonColor(
    bool is_root_app_grid_page_switcher) const {
  return DeprecatedGetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor,
      is_root_app_grid_page_switcher ? SkColorSetARGB(255, 232, 234, 237)
                                     : SkColorSetA(SK_ColorBLACK, 138));
}

SkColor AppListColorProviderImpl::GetSearchBoxIconColor(
    SkColor default_color) const {
  return DeprecatedGetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor, default_color);
}

SkColor AppListColorProviderImpl::GetFolderBackgroundColor(
    SkColor default_color) const {
  return DeprecatedGetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80, default_color);
}

SkColor AppListColorProviderImpl::GetFolderBubbleColor() const {
  return DeprecatedGetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive,
      SkColorSetA(gfx::kGoogleGrey100, 0x7A));
}

SkColor AppListColorProviderImpl::GetFolderTitleTextColor(
    SkColor default_color) const {
  return DeprecatedGetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary, default_color);
}

SkColor AppListColorProviderImpl::GetFolderHintTextColor() const {
  return DeprecatedGetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary,
      /*default_color*/ gfx::kGoogleGrey600);
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
  return DeprecatedGetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive,
      /*default_color*/ SkColorSetRGB(0xF2, 0xF2, 0xF2));
}

SkColor AppListColorProviderImpl::GetSeparatorColor() const {
  return DeprecatedGetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparatorColor,
      /*default_color*/ SkColorSetA(gfx::kGoogleGrey900, 0x24));
}

SkColor AppListColorProviderImpl::GetFocusRingColor() const {
  return DeprecatedGetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorActive,
      gfx::kGoogleBlue300);
}

SkColor AppListColorProviderImpl::GetFolderItemFocusRingColor() const {
  return DeprecatedGetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorActive,
      gfx::kGoogleBlue600);
}

SkColor AppListColorProviderImpl::GetPrimaryIconColor(
    SkColor default_color) const {
  return DeprecatedGetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary, default_color);
}

float AppListColorProviderImpl::GetFolderBackgrounBlurSigma() const {
  return static_cast<float>(AshColorProvider::LayerBlurSigma::kBlurDefault);
}

SkColor AppListColorProviderImpl::GetRippleAttributesBaseColor(
    SkColor bg_color) const {
  return ash_color_provider_->GetRippleAttributes(bg_color).base_color;
}

float AppListColorProviderImpl::GetRippleAttributesInkDropOpacity(
    SkColor bg_color) const {
  return ash_color_provider_->GetRippleAttributes(bg_color).inkdrop_opacity;
}

float AppListColorProviderImpl::GetRippleAttributesHighlightOpacity(
    SkColor bg_color) const {
  return ash_color_provider_->GetRippleAttributes(bg_color).highlight_opacity;
}

}  // namespace ash

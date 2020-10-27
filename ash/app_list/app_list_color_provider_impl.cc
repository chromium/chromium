// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_color_provider_impl.h"

#include "ash/style/ash_color_provider.h"
#include "ash/style/default_colors.h"

namespace ash {

AppListColorProviderImpl::AppListColorProviderImpl()
    : ash_color_provider_(AshColorProvider::Get()) {}

AppListColorProviderImpl::~AppListColorProviderImpl() = default;

SkColor AppListColorProviderImpl::GetExpandArrowInkDropBaseColor() const {
  return DeprecatedGetInkDropBaseColor(
      /*default_color*/ SkColorSetARGB(0x14, 0xFF, 0xFF, 0xFF));
}

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

SkColor AppListColorProviderImpl::GetAppListBackgroundColor() const {
  return DeprecatedGetShieldLayerColor(
      AshColorProvider::ShieldLayerType::kShield80,
      /*default_color*/ gfx::kGoogleGrey900);
}

SkColor AppListColorProviderImpl::GetSearchBoxBackgroundColor() const {
  return DeprecatedGetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive,
      SK_ColorWHITE);
}

SkColor AppListColorProviderImpl::GetSearchBoxCardBackgroundColor() const {
  // Set solid color background to avoid broken text. See crbug.com/746563.
  return DeprecatedGetBaseLayerColor(AshColorProvider::BaseLayerType::kOpaque,
                                     /*default_color*/ SK_ColorWHITE);
}

SkColor AppListColorProviderImpl::GetSearchBoxPlaceholderTextColor() const {
  return DeprecatedGetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary,
      /*default_color*/ SkColorSetARGB(0xDE, 0x00, 0x00, 0x00));
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
  return DeprecatedGetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive,
      /*default_color*/ SkColorSetA(gfx::kGoogleGrey100, 0x14));
}

SkColor AppListColorProviderImpl::GetSuggestionChipTextColor() const {
  return DeprecatedGetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary,
      /*default_color*/ gfx::kGoogleGrey100);
}

SkColor AppListColorProviderImpl::GetAppListItemTextColor() const {
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

SkColor AppListColorProviderImpl::GetPageSwitcherInkDropBaseColor(
    bool is_root_app_grid_page_switcher) const {
  return DeprecatedGetInkDropRippleColor(
      is_root_app_grid_page_switcher
          ? SkColorSetA(SkColorSetRGB(241, 243, 244), 15)
          : SkColorSetA(SkColorSetARGB(255, 95, 99, 104), 8));
}

SkColor AppListColorProviderImpl::GetPageSwitcherInkDropHighlightColor(
    bool is_root_app_grid_page_switcher) const {
  return DeprecatedGetInkDropHighlightColor(
      is_root_app_grid_page_switcher
          ? SkColorSetA(SkColorSetRGB(241, 243, 244), 20)
          : SkColorSetA(SkColorSetARGB(255, 95, 99, 104), 24));
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

SkColor AppListColorProviderImpl::GetFolderNameBackgroundColor(
    bool active) const {
  if (!active)
    return SK_ColorTRANSPARENT;

  return DeprecatedGetInkDropRippleColor(/*default_color*/ gfx::kGoogleGrey100);
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

SkColor AppListColorProviderImpl::GetSearchResultViewInkDropColor() const {
  return DeprecatedGetInkDropRippleColor(
      SkColorSetA(gfx::kGoogleGrey900, 0x14));
}

SkColor AppListColorProviderImpl::GetSearchResultViewHighlightColor() const {
  return DeprecatedGetInkDropHighlightColor(
      SkColorSetA(gfx::kGoogleGrey900, 0x12));
}

float AppListColorProviderImpl::GetFolderBackgrounBlurSigma() const {
  return static_cast<float>(AshColorProvider::LayerBlurSigma::kBlurDefault);
}

}  // namespace ash

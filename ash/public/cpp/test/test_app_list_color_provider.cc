// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_app_list_color_provider.h"

#include "ui/gfx/color_palette.h"

namespace ash {

SkColor TestAppListColorProvider::GetExpandArrowIconBaseColor() const {
  return gfx::kGoogleGrey200;
}

SkColor TestAppListColorProvider::GetExpandArrowIconBackgroundColor() const {
  return SkColorSetA(SK_ColorWHITE, 0x1A);
}

SkColor TestAppListColorProvider::GetAppListBackgroundColor(
    bool is_tablet_mode) const {
  return SkColorSetA(gfx::kGoogleGrey900,
                     is_tablet_mode ? 0x66 /*40%*/ : 0xCD /*80%*/);
}

SkColor TestAppListColorProvider::GetSearchBoxBackgroundColor() const {
  return gfx::kGoogleGrey900;
}

SkColor TestAppListColorProvider::GetSearchBoxCardBackgroundColor() const {
  return gfx::kGoogleGrey900;
}

SkColor TestAppListColorProvider::GetSearchBoxTextColor(
    SkColor default_color) const {
  return gfx::kGoogleGrey200;
}

SkColor TestAppListColorProvider::GetSearchBoxSecondaryTextColor(
    SkColor default_color) const {
  return gfx::kGoogleGrey500;
}

SkColor TestAppListColorProvider::GetSuggestionChipBackgroundColor() const {
  return SkColorSetA(SK_ColorWHITE, 0x1A);
}

SkColor TestAppListColorProvider::GetSuggestionChipTextColor() const {
  return gfx::kGoogleGrey200;
}

SkColor TestAppListColorProvider::GetAppListItemTextColor(
    bool is_in_folder) const {
  return gfx::kGoogleGrey200;
}

SkColor TestAppListColorProvider::GetFolderBackgroundColor(
    SkColor default_color) const {
  return gfx::kGoogleGrey900;
}

SkColor TestAppListColorProvider::GetFolderBubbleColor() const {
  return SkColorSetA(gfx::kGoogleGrey100, 0x7A);
}

SkColor TestAppListColorProvider::GetPageSwitcherButtonColor(
    bool is_root_app_grid_page_switcher) const {
  return gfx::kGoogleGrey700;
}

SkColor TestAppListColorProvider::GetSearchBoxIconColor(
    SkColor default_color) const {
  return gfx::kGoogleGrey200;
}

SkColor TestAppListColorProvider::GetFolderTitleTextColor(
    SkColor default_color) const {
  return gfx::kGoogleGrey200;
}

SkColor TestAppListColorProvider::GetFolderHintTextColor() const {
  return gfx::kGoogleGrey500;
}

SkColor TestAppListColorProvider::GetFolderNameBorderColor(bool active) const {
  if (!active)
    return SK_ColorTRANSPARENT;

  return SkColorSetA(SK_ColorBLACK, 0x0F);
}

SkColor TestAppListColorProvider::GetFolderNameSelectionColor() const {
  return SkColorSetA(SK_ColorBLACK, 0x0F);
}

SkColor TestAppListColorProvider::GetContentsBackgroundColor() const {
  return gfx::kGoogleGrey200;
}

SkColor TestAppListColorProvider::GetSeparatorColor() const {
  return SkColorSetA(SK_ColorWHITE, 0x24);
}

SkColor TestAppListColorProvider::GetFocusRingColor() const {
  return gfx::kGoogleBlue300;
}

SkColor TestAppListColorProvider::GetFolderItemFocusRingColor() const {
  return gfx::kGoogleBlue600;
}

SkColor TestAppListColorProvider::GetPrimaryIconColor(
    SkColor default_color) const {
  return default_color;
}

float TestAppListColorProvider::GetFolderBackgrounBlurSigma() const {
  return 30.0f;
}

SkColor TestAppListColorProvider::GetRippleAttributesBaseColor(
    SkColor bg_color) const {
  return SK_ColorWHITE;
}

float TestAppListColorProvider::GetRippleAttributesInkDropOpacity(
    SkColor bg_color) const {
  return 0.08f;
}

float TestAppListColorProvider::GetRippleAttributesHighlightOpacity(
    SkColor bg_color) const {
  return 0.08f;
}

}  // namespace ash

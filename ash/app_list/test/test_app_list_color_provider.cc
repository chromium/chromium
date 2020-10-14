// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/test/test_app_list_color_provider.h"

#include "ui/gfx/color_palette.h"

namespace ash {

SkColor TestAppListColorProvider::GetExpandArrowInkDropBaseColor() const {
  return SK_ColorWHITE;
}

SkColor TestAppListColorProvider::GetExpandArrowIconBaseColor() const {
  return gfx::kGoogleGrey200;
}

SkColor TestAppListColorProvider::GetExpandArrowIconBackgroundColor() const {
  return SkColorSetA(SK_ColorWHITE, 0x1A);
}

SkColor TestAppListColorProvider::GetAppListBackgroundColor() const {
  return gfx::kGoogleGrey900;
}

SkColor TestAppListColorProvider::GetSearchBoxBackgroundColor() const {
  return gfx::kGoogleGrey900;
}

SkColor TestAppListColorProvider::GetSearchBoxCardBackgroundColor() const {
  return gfx::kGoogleGrey900;
}

SkColor TestAppListColorProvider::GetSearchBoxPlaceholderTextColor() const {
  return gfx::kGoogleGrey500;
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

SkColor TestAppListColorProvider::GetAppListItemTextColor() const {
  return gfx::kGoogleGrey200;
}

SkColor TestAppListColorProvider::GetFolderBackgroundColor(
    SkColor default_color) const {
  return gfx::kGoogleGrey900;
}

SkColor TestAppListColorProvider::GetPageSwitcherButtonColor(
    bool is_root_app_grid_page_switcher) const {
  return gfx::kGoogleGrey700;
}

SkColor TestAppListColorProvider::GetPageSwitcherInkDropBaseColor(
    bool is_root_app_grid_page_switcher) const {
  return SkColorSetA(SK_ColorBLACK, 0x0F);
}

SkColor TestAppListColorProvider::GetPageSwitcherInkDropHighlightColor(
    bool is_root_app_grid_page_switcher) const {
  return SkColorSetA(SK_ColorBLACK, 0x0F);
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

SkColor TestAppListColorProvider::GetFolderNameBackgroundColor(
    bool active) const {
  if (!active)
    return SK_ColorTRANSPARENT;

  return SkColorSetA(SK_ColorBLACK, 0x0F);
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

SkColor TestAppListColorProvider::GetSearchResultViewHighlightColor() const {
  return SkColorSetA(SK_ColorWHITE, 0x12);
}

SkColor TestAppListColorProvider::GetSearchResultViewInkDropColor() const {
  return SkColorSetA(SK_ColorWHITE, 0x17);
}

float TestAppListColorProvider::GetFolderBackgrounBlurSigma() const {
  return 30.0f;
}

}  // namespace ash

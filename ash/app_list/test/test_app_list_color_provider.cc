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
  return SK_ColorWHITE;
}

SkColor TestAppListColorProvider::GetSearchBoxPlaceholderTextColor() const {
  return gfx::kGoogleGrey500;
}

SkColor TestAppListColorProvider::GetSearchBoxTextColor() const {
  return gfx::kGoogleGrey900;
}

SkColor TestAppListColorProvider::GetSuggestionChipBackgroundColor() const {
  return SkColorSetA(SK_ColorWHITE, 0x1A);
}

SkColor TestAppListColorProvider::GetSuggestionChipTextColor() const {
  return gfx::kGoogleGrey900;
}

SkColor TestAppListColorProvider::GetAppListItemTextColor() const {
  return gfx::kGoogleGrey900;
}

SkColor TestAppListColorProvider::GetPageSwitcherButtonColor() const {
  return gfx::kGoogleGrey900;
}

SkColor TestAppListColorProvider::GetPageSwitcherInkDropBaseColor() const {
  return SkColorSetA(SK_ColorBLACK, 0x0F);
}

SkColor TestAppListColorProvider::GetPageSwitcherInkDropHighlightColor() const {
  return SkColorSetA(SK_ColorBLACK, 0x0F);
}

}  // namespace ash

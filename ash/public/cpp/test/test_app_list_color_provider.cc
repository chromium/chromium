// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_app_list_color_provider.h"

#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"

namespace ash {

SkColor TestAppListColorProvider::GetFolderBackgroundColor(
    const views::Widget* app_list_widget) const {
  return gfx::kGoogleGrey900;
}

SkColor TestAppListColorProvider::GetPageSwitcherButtonColor(
    const views::Widget* app_list_widget) const {
  return gfx::kGoogleGrey700;
}

SkColor TestAppListColorProvider::GetFolderNameBorderColor(
    bool active,
    const views::Widget* app_list_widget) const {
  if (!active)
    return SK_ColorTRANSPARENT;

  return SkColorSetA(SK_ColorBLACK, 0x0F);
}

SkColor TestAppListColorProvider::GetFolderNotificationBadgeColor(
    const views::Widget* app_list_widget) const {
  return SK_ColorWHITE;
}

SkColor TestAppListColorProvider::GetContentsBackgroundColor(
    const views::Widget* app_list_widget) const {
  return gfx::kGoogleGrey200;
}

SkColor TestAppListColorProvider::GetGridBackgroundCardActiveColor(
    const views::Widget* app_list_widget) const {
  return SkColorSetA(SK_ColorWHITE, 26 /* 10% */);
}

SkColor TestAppListColorProvider::GetGridBackgroundCardInactiveColor(
    const views::Widget* app_list_widget) const {
  return SkColorSetA(SK_ColorWHITE, 41 /* 16% */);
}

SkColor TestAppListColorProvider::GetFocusRingColor(
    const views::Widget* app_list_widget) const {
  return gfx::kGoogleBlue600;
}

SkColor TestAppListColorProvider::GetInkDropBaseColor(
    const views::Widget* app_list_widget,
    SkColor bg_color) const {
  return SK_ColorWHITE;
}

float TestAppListColorProvider::GetInkDropOpacity(
    const views::Widget* app_list_widget,
    SkColor bg_color) const {
  return 0.08f;
}

SkColor TestAppListColorProvider::GetSearchResultViewHighlightColor(
    const views::Widget* widget) const {
  return SkColorSetA(SK_ColorWHITE, 0x0D);
}

}  // namespace ash

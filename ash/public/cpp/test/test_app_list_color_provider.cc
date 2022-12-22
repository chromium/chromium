// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_app_list_color_provider.h"

#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"

namespace ash {

SkColor TestAppListColorProvider::GetPageSwitcherButtonColor(
    const views::Widget* app_list_widget) const {
  return gfx::kGoogleGrey700;
}

SkColor TestAppListColorProvider::GetFolderNotificationBadgeColor(
    const views::Widget* app_list_widget) const {
  return SK_ColorWHITE;
}

SkColor TestAppListColorProvider::GetGridBackgroundCardActiveColor(
    const views::Widget* app_list_widget) const {
  return SkColorSetA(SK_ColorWHITE, 26 /* 10% */);
}

SkColor TestAppListColorProvider::GetGridBackgroundCardInactiveColor(
    const views::Widget* app_list_widget) const {
  return SkColorSetA(SK_ColorWHITE, 41 /* 16% */);
}

}  // namespace ash

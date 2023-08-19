// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_ICON_CONSTANTS_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_ICON_CONSTANTS_H_

#include "ui/gfx/color_palette.h"

namespace app_list {

// Result icon dimension constants.
constexpr int kFaviconDimension = 18;
constexpr int kThumbnailDimension = 28;
constexpr int kSystemIconDimension = 20;
constexpr int kAnswerCardIconDimension = 28;
constexpr int kSystemAnswerCardIconDimension = 32;
constexpr int kAppIconDimension = 32;
constexpr int kImageIconDimension = 28;
constexpr int kImageSearchWidth = 240;
constexpr int kImageSearchHeight = 160;

SkColor GetGenericIconColor();

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_ICON_CONSTANTS_H_

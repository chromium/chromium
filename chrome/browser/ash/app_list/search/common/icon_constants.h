// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_ICON_CONSTANTS_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_ICON_CONSTANTS_H_

#include <stdint.h>

#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"

using SkColor = uint32_t;

namespace app_list {

// Result icon dimension constants.
inline constexpr int kFaviconDimension = 18;
inline constexpr int kThumbnailDimension = 28;
inline constexpr int kSystemIconDimension = 20;
inline constexpr int kAnswerCardIconDimension = 28;
inline constexpr int kSystemAnswerCardIconDimension = 32;
inline constexpr int kAppIconDimension = 32;
inline constexpr int kImageIconDimension = 28;
inline constexpr int kImageSearchWidth = 240;
inline constexpr int kImageSearchHeight = 160;
inline constexpr ui::ColorId kGenericIconColorId = cros_tokens::kColorPrimary;

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_ICON_CONSTANTS_H_

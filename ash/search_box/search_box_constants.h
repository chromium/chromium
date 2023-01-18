// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SEARCH_BOX_SEARCH_BOX_CONSTANTS_H_
#define ASH_SEARCH_BOX_SEARCH_BOX_CONSTANTS_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"

namespace ash {

// The horizontal padding of the box layout of the search box.
constexpr int kSearchBoxPadding = 12;

// The default background color of the search box.
constexpr SkColor kSearchBoxBackgroundDefault = SK_ColorWHITE;

// The background border corner radius of the search box.
constexpr int kSearchBoxBorderCornerRadius = 24;

// The background border corner radius of the expanded search box.
constexpr int kSearchBoxBorderCornerRadiusSearchResult = 20;

// The background border corner radius of the active/expanded search box.
constexpr int kExpandedSearchBoxCornerRadius = 28;

// Preferred height of search box.
constexpr int kSearchBoxPreferredHeight = 48;

// The size of the icon in the search box.
constexpr int kBubbleLauncherSearchBoxIconSize = 20;

// The size of the image button in the search box.
constexpr int kBubbleLauncherSearchBoxButtonSizeDip = 36;

// Color of placeholder text in zero query state.
constexpr SkColor kZeroQuerySearchboxColor =
    SkColorSetARGB(0x8A, 0x00, 0x00, 0x00);

}  // namespace ash

#endif  // ASH_SEARCH_BOX_SEARCH_BOX_CONSTANTS_H_

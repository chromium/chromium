// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_PINE_CONSTANTS_H_
#define ASH_WM_WINDOW_RESTORE_PINE_CONSTANTS_H_

#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

namespace ash::pine {

// The maximum number of windows that can be displayed in the Pine container
// before the overflow view must be used.
inline constexpr int kMaxItems = 4;

// The starting index of the `PineItemsOverflowView` in the
// `PineItemsContainerView`. Also corresponds to the number of regular window
// items displayed alongside `PineItemsOverflowView`.
inline constexpr int kOverflowMinThreshold = kMaxItems - 1;

// The maximum number of elements that can be shown inside the screenshot icon
// row.
inline constexpr int kScreenshotIconRowMaxElements = 5;

// The preferred size of the rounded background behind each window's icon.
inline constexpr gfx::Size kItemIconBackgroundPreferredSize(40, 40);

// The spacing between the window icon and other information (title and tab
// icons).
inline constexpr int kItemChildSpacing = 10;

// The size of each window's title.
inline constexpr int kItemTitleFontSize = 16;

// The spacing between each Pine item that represents a window (or group of
// overflowing windows).
inline constexpr int kItemsContainerChildSpacing = 10;

// The insets for `PineItemsContainerView`.
inline constexpr gfx::Insets kItemsContainerInsets = gfx::Insets::VH(15, 15);

// The desired image size for each window's icon.
inline constexpr int kAppImageSize = 64;

// The text color for `PineItemView` and `PineItemOverflowView`.
inline constexpr ui::ColorId kPineItemTextColorId =
    cros_tokens::kCrosSysOnSurface;

// The background color behind each app displayed in `PineItemsContainerView`.
inline constexpr ui::ColorId kIconBackgroundColorId =
    cros_tokens::kCrosSysSystemOnBase;

// Width of the preview container inside the pine dialog. Contents of the
// preview can be either the items view or the screenshot.
inline constexpr int kPreviewContainerWidth = 344;

// Corner radius of the preview container inside the pine dialog.
inline constexpr int kPreviewContainerRadius = 12;

inline constexpr gfx::Size kOverflowIconPreferredSize(20, 20);

// Constants for the icon row inside the screenshot preview.
inline constexpr int kScreenshotIconRowChildSpacing = 4;
inline constexpr int kScreenshotIconRowIconSize = 20;
inline constexpr gfx::Size kScreenshotIconRowImageViewSize(20, 20);
inline constexpr int kScreenshotFaviconSpacing = 2;

inline constexpr char kSuggestionsNudgeId[] = "PineSuggestionsNudge";
inline constexpr char kOnboardingToastId[] = "PineOnboardingTabletToast";

// IDs used for the views that compose the pine dialog UI. Use these for easy
// access to the views during the unit tests. Note that these IDs are only
// guaranteed to be unique inside `PineContentsView`. We don't use an enum class
// to avoid too many explicit casts at callsites.
enum ViewID : int {
  kRestoreButtonID = 1,
  kCancelButtonID,
  kSettingsButtonID,
  kOverflowViewID,
  kOverflowTopRowViewID,
  kOverflowBottomRowViewID,
  kOverflowImageViewID,
  kScreenshotImageViewID,
  kScreenshotIconRowViewID,
  kFaviconContainerViewID,
  kPreviewContainerViewID,
  kItemImageViewID,
  kItemViewID,
};

}  // namespace ash::pine

#endif  // ASH_WM_WINDOW_RESTORE_PINE_CONSTANTS_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CONSTANTS_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CONSTANTS_H_

#include "ash/public/cpp/app_menu_constants.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

// Appearance.
constexpr int kHoldingSpaceBubbleContainerChildSpacing = 8;
constexpr int kHoldingSpaceBubbleWidth = 360;
constexpr gfx::Insets kHoldingSpaceChildBubblePadding(16);
constexpr int kHoldingSpaceChildBubbleChildSpacing = 16;
constexpr int kHoldingSpaceChipIconSize = 24;
constexpr int kHoldingSpaceContextMenuMargin = 8;
constexpr int kHoldingSpaceCornerRadius = 8;
constexpr int kHoldingSpaceDownloadsChevronIconSize = 20;
constexpr int kHoldingSpaceDownloadsHeaderSpacing = 16;
constexpr int kHoldingSpaceFocusInsets = -2;
constexpr int kHoldingSpaceIconSize = 20;
constexpr gfx::Insets kHoldingSpaceScreenCapturePadding(8);
constexpr gfx::Size kHoldingSpaceScreenCapturePinButtonSize(24, 24);
constexpr gfx::Size kHoldingSpaceScreenCapturePlayIconSize(32, 32);
constexpr int kHoldingSpaceScreenCaptureSpacing = 8;
constexpr gfx::Size kHoldingSpaceScreenCaptureSize(104, 80);
constexpr gfx::Insets kHoldingSpaceScreenCapturesContainerPadding(8, 0);
constexpr int kHoldingSpaceSectionChildSpacing = 16;
constexpr float kHoldingSpaceSelectedOverlayOpacity = 0.24f;
constexpr int kHoldingSpaceTrayIconMaxVisiblePreviews = 3;
constexpr int kHoldingSpaceTrayIconPreviewSize = 32;
constexpr int kHoldingSpaceTrayIconSize = 20;

// Context menu commands.
enum HoldingSpaceCommandId {
  kCopyImageToClipboard,
  kHidePreviews,
  kPinItem,
  kShowInFolder,
  kShowPreviews,
  kUnpinItem,
  kMaxValue = kUnpinItem
};

// View IDs.
constexpr int kHoldingSpaceFilesAppChipId = 1;
constexpr int kHoldingSpaceItemPinButtonId = 2;
constexpr int kHoldingSpacePinnedFilesBubbleId = 3;
constexpr int kHoldingSpaceRecentFilesBubbleId = 4;
constexpr int kHoldingSpaceScreenCapturePlayIconId = 5;
constexpr int kHoldingSpaceTrayDefaultIconId = 6;
constexpr int kHoldingSpaceTrayPreviewsIconId = 7;

// The maximum allowed age for files restored into the holding space model.
// Note that this is not enforced for pinned items.
constexpr base::TimeDelta kMaxFileAge = base::TimeDelta::FromDays(1);

// The maximum allowed number of downloads to display in holding space UI.
constexpr size_t kMaxDownloads = 2u;

// The maximum allowed number of screen captures to display in holding space UI.
constexpr size_t kMaxScreenCaptures = 3u;

// Mime type with wildcard which matches all image types.
constexpr char kMimeTypeImage[] = "image/*";

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CONSTANTS_H_

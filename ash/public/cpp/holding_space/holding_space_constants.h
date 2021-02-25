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
constexpr int kHoldingSpaceFocusCornerRadius = 11;
constexpr int kHoldingSpaceFocusInsets = -4;
constexpr int kHoldingSpaceIconSize = 20;
constexpr gfx::Size kHoldingSpaceScreenCaptureSize(104, 80);
constexpr int kHoldingSpaceSectionChildSpacing = 16;
constexpr float kHoldingSpaceSelectedOverlayOpacity = 0.3f;
constexpr int kHoldingSpaceTrayIconMaxVisiblePreviews = 3;
constexpr int kHoldingSpaceTrayIconDefaultPreviewSize = 32;
constexpr int kHoldingSpaceTrayIconSmallPreviewSize = 28;
constexpr int kHoldingSpaceTrayIconSize = 20;

// Context menu commands.
enum class HoldingSpaceCommandId {
  kCopyImageToClipboard,
  kHidePreviews,
  kRemoveItem,
  kPinItem,
  kShowInFolder,
  kShowPreviews,
  kUnpinItem,
};

// View IDs.
constexpr int kHoldingSpaceDownloadsSectionHeaderId = 1;
constexpr int kHoldingSpaceFilesAppChipId = 2;
constexpr int kHoldingSpaceItemCheckmarkId = 3;
constexpr int kHoldingSpaceItemImageId = 4;
constexpr int kHoldingSpaceItemPinButtonId = 5;
constexpr int kHoldingSpacePinnedFilesBubbleId = 6;
constexpr int kHoldingSpaceRecentFilesBubbleId = 7;
constexpr int kHoldingSpaceScreenCapturePlayIconId = 8;
constexpr int kHoldingSpaceTrayDefaultIconId = 9;
constexpr int kHoldingSpaceTrayDropTargetOverlayId = 10;
constexpr int kHoldingSpaceTrayPreviewsIconId = 11;

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

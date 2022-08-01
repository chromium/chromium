// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CONSTANTS_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CONSTANTS_H_

#include "ash/public/cpp/app_menu_constants.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

// Appearance.
constexpr int kHoldingSpaceBubbleContainerChildSpacing = 8;
constexpr int kHoldingSpaceBubbleWidth = 360;
constexpr gfx::Insets kHoldingSpaceChildBubblePadding(16);
constexpr int kHoldingSpaceChildBubbleChildSpacing = 16;
constexpr int kHoldingSpaceChipCountPerRow = 2;
constexpr int kHoldingSpaceChipIconSize = 24;
constexpr int kHoldingSpaceCornerRadius = 8;
constexpr int kHoldingSpaceDownloadsChevronIconSize = 20;
constexpr int kHoldingSpaceDownloadsHeaderSpacing = 16;
constexpr int kHoldingSpaceFocusCornerRadius = 11;
constexpr int kHoldingSpaceFocusInsets = -4;
constexpr int kHoldingSpaceIconSize = 20;
constexpr gfx::Size kHoldingSpaceScreenCaptureSize(104, 80);
constexpr int kHoldingSpaceSectionChildSpacing = 16;
constexpr int kHoldingSpaceSectionContainerChildSpacing = 8;
constexpr float kHoldingSpaceSelectedOverlayOpacity = 0.3f;
constexpr int kHoldingSpaceTrayIconMaxVisiblePreviews = 3;
constexpr int kHoldingSpaceTrayIconDefaultPreviewSize = 32;
constexpr int kHoldingSpaceTrayIconSmallPreviewSize = 28;
constexpr int kHoldingSpaceTrayIconSize = 20;

// Context menu commands.
enum class HoldingSpaceCommandId {
  kMinValue = 1,  // NOTE: Zero is used when command id is unset.

  // Core item commands.
  kCopyImageToClipboard = kMinValue,
  kRemoveItem,
  kPinItem,
  kShowInFolder,
  kUnpinItem,

  // In-progress item commands.
  kCancelItem,
  kResumeItem,
  kPauseItem,

  // Tray commands.
  kHidePreviews,
  kShowPreviews,

  kMaxValue = kShowPreviews,
};

// View IDs.
constexpr int kHoldingSpaceDownloadsSectionHeaderId = 1;
constexpr int kHoldingSpaceFilesAppChipId = 2;
constexpr int kHoldingSpaceItemCancelButtonId = 3;
constexpr int kHoldingSpaceItemCheckmarkId = 4;
constexpr int kHoldingSpaceItemImageId = 5;
constexpr int kHoldingSpaceItemPauseButtonId = 6;
constexpr int kHoldingSpaceItemPinButtonId = 7;
constexpr int kHoldingSpaceItemPrimaryActionContainerId = 8;
constexpr int kHoldingSpaceItemSecondaryActionContainerId = 9;
constexpr int kHoldingSpaceItemPrimaryChipLabelId = 10;
constexpr int kHoldingSpaceItemSecondaryChipLabelId = 11;
constexpr int kHoldingSpaceItemResumeButtonId = 12;
constexpr int kHoldingSpacePinnedFilesBubbleId = 13;
constexpr int kHoldingSpaceRecentFilesBubbleId = 14;
constexpr int kHoldingSpaceScreenCapturePlayIconId = 15;
constexpr int kHoldingSpaceTrayDefaultIconId = 16;
constexpr int kHoldingSpaceTrayDropTargetOverlayId = 17;
constexpr int kHoldingSpaceTrayPreviewsIconId = 18;

// The maximum allowed age for files restored into the holding space model.
// Note that this is not enforced for pinned items.
constexpr base::TimeDelta kMaxFileAge = base::Days(1);

// The maximum allowed number of downloads to display in holding space UI.
constexpr size_t kMaxDownloads = 4u;

// The maximum allowed number of screen captures to display in holding space UI.
constexpr size_t kMaxScreenCaptures = 3u;

// Mime type with wildcard which matches all image types.
constexpr char kMimeTypeImage[] = "image/*";

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CONSTANTS_H_

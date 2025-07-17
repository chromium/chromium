// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CONSTANTS_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CONSTANTS_H_

#include "base/time/time.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

// Appearance.
constexpr float kHoldingSpaceSelectedOverlayOpacity = 0.3f;
constexpr gfx::Insets kHoldingSpaceChildBubblePadding(16);
constexpr gfx::Size kHoldingSpaceScreenCaptureSize(104, 80);
constexpr int kHoldingSpaceBubbleContainerChildSpacing = 8;
constexpr int kHoldingSpaceChildBubbleChildSpacing = 16;
constexpr int kHoldingSpaceChipCountPerRow = 2;
constexpr int kHoldingSpaceChipIconSize = 24;
constexpr int kHoldingSpaceCornerRadius = 8;
constexpr int kHoldingSpaceFocusCornerRadius = 11;
constexpr int kHoldingSpaceFocusInsets = -4;
constexpr int kHoldingSpaceIconSize = 20;
constexpr int kHoldingSpaceSectionChevronIconSize = 20;
constexpr int kHoldingSpaceSectionChildSpacing = 16;
constexpr int kHoldingSpaceSectionContainerChildSpacing = 8;
constexpr int kHoldingSpaceSectionHeaderSpacing = 16;
constexpr int kHoldingSpaceTrayIconDefaultPreviewSize = 32;
constexpr int kHoldingSpaceTrayIconMaxVisiblePreviews = 3;
constexpr int kHoldingSpaceTrayIconSize = 20;
constexpr int kHoldingSpaceTrayIconSmallPreviewSize = 28;

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
  kOpenItem,
  kPauseItem,
  kResumeItem,
  kViewItemDetailsInBrowser,

  // Tray commands.
  kHidePreviews,
  kShowPreviews,

  kMaxValue = kShowPreviews,
};

// View IDs.
inline constexpr int kHoldingSpaceDownloadsSectionHeaderId = 1;
inline constexpr int kHoldingSpaceItemCancelButtonId = 2;
inline constexpr int kHoldingSpaceItemCheckmarkId = 3;
inline constexpr int kHoldingSpaceItemImageId = 4;
inline constexpr int kHoldingSpaceItemPauseButtonId = 5;
inline constexpr int kHoldingSpaceItemPinButtonId = 6;
inline constexpr int kHoldingSpaceItemPrimaryActionContainerId = 7;
inline constexpr int kHoldingSpaceItemPrimaryChipLabelId = 8;
inline constexpr int kHoldingSpaceItemResumeButtonId = 9;
inline constexpr int kHoldingSpaceItemSecondaryActionContainerId = 10;
inline constexpr int kHoldingSpaceItemSecondaryChipLabelId = 11;
inline constexpr int kHoldingSpacePinnedFilesBubbleId = 12;
inline constexpr int kHoldingSpacePinnedFilesSectionId = 13;
inline constexpr int kHoldingSpacePinnedFilesSectionPlaceholderGSuiteIconsId =
    14;
inline constexpr int kHoldingSpacePinnedFilesSectionPlaceholderLabelId = 15;
inline constexpr int kHoldingSpaceRecentFilesBubbleId = 16;
inline constexpr int kHoldingSpaceRecentFilesPlaceholderId = 17;
inline constexpr int kHoldingSpaceScreenCaptureOverlayIconId = 18;
inline constexpr int kHoldingSpaceSuggestionsChevronIconId = 19;
inline constexpr int kHoldingSpaceSuggestionsSectionContainerId = 20;
inline constexpr int kHoldingSpaceSuggestionsSectionHeaderId = 21;
inline constexpr int kHoldingSpaceSuggestionsSectionId = 22;
inline constexpr int kHoldingSpaceTrayDefaultIconId = 23;
inline constexpr int kHoldingSpaceTrayDropTargetOverlayId = 24;
inline constexpr int kHoldingSpaceTrayPreviewsIconId = 25;

// The maximum allowed age for files restored into the holding space model.
// Note that this is not enforced for pinned items.
constexpr base::TimeDelta kMaxFileAge = base::Days(1);

// Mime type with wildcard which matches all image types.
constexpr char kMimeTypeImage[] = "image/*";

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CONSTANTS_H_

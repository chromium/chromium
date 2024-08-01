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
constexpr int kHoldingSpaceDownloadsSectionHeaderId = 1;
constexpr int kHoldingSpaceFilesAppChipId = 2;
constexpr int kHoldingSpaceItemCancelButtonId = 3;
constexpr int kHoldingSpaceItemCheckmarkId = 4;
constexpr int kHoldingSpaceItemImageId = 5;
constexpr int kHoldingSpaceItemPauseButtonId = 6;
constexpr int kHoldingSpaceItemPinButtonId = 7;
constexpr int kHoldingSpaceItemPrimaryActionContainerId = 8;
constexpr int kHoldingSpaceItemPrimaryChipLabelId = 9;
constexpr int kHoldingSpaceItemResumeButtonId = 10;
constexpr int kHoldingSpaceItemSecondaryActionContainerId = 11;
constexpr int kHoldingSpaceItemSecondaryChipLabelId = 12;
constexpr int kHoldingSpacePinnedFilesBubbleId = 13;
constexpr int kHoldingSpacePinnedFilesSectionId = 14;
constexpr int kHoldingSpacePinnedFilesSectionPlaceholderGSuiteIconsId = 15;
constexpr int kHoldingSpacePinnedFilesSectionPlaceholderLabelId = 16;
constexpr int kHoldingSpaceRecentFilesBubbleId = 17;
constexpr int kHoldingSpaceRecentFilesPlaceholderId = 18;
constexpr int kHoldingSpaceScreenCaptureOverlayIconId = 19;
constexpr int kHoldingSpaceSuggestionsChevronIconId = 20;
constexpr int kHoldingSpaceSuggestionsSectionContainerId = 21;
constexpr int kHoldingSpaceSuggestionsSectionHeaderId = 22;
constexpr int kHoldingSpaceSuggestionsSectionId = 23;
constexpr int kHoldingSpaceTrayDefaultIconId = 24;
constexpr int kHoldingSpaceTrayDropTargetOverlayId = 25;
constexpr int kHoldingSpaceTrayPreviewsIconId = 26;

// The maximum allowed age for files restored into the holding space model.
// Note that this is not enforced for pinned items.
constexpr base::TimeDelta kMaxFileAge = base::Days(1);

// Mime type with wildcard which matches all image types.
constexpr char kMimeTypeImage[] = "image/*";

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CONSTANTS_H_

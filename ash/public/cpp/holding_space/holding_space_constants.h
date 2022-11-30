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
constexpr int kHoldingSpaceBubbleWidth = 360;
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
constexpr int kHoldingSpaceHeaderLabelId = 3;
constexpr int kHoldingSpaceItemCancelButtonId = 4;
constexpr int kHoldingSpaceItemCheckmarkId = 5;
constexpr int kHoldingSpaceItemImageId = 6;
constexpr int kHoldingSpaceItemPauseButtonId = 7;
constexpr int kHoldingSpaceItemPinButtonId = 8;
constexpr int kHoldingSpaceItemPrimaryActionContainerId = 9;
constexpr int kHoldingSpaceItemPrimaryChipLabelId = 10;
constexpr int kHoldingSpaceItemResumeButtonId = 11;
constexpr int kHoldingSpaceItemSecondaryActionContainerId = 12;
constexpr int kHoldingSpaceItemSecondaryChipLabelId = 13;
constexpr int kHoldingSpacePinnedFilesBubbleId = 14;
constexpr int kHoldingSpacePinnedFilesSectionId = 15;
constexpr int kHoldingSpacePinnedFilesSectionPlaceholderGSuiteIconsId = 16;
constexpr int kHoldingSpacePinnedFilesSectionPlaceholderLabelId = 17;
constexpr int kHoldingSpaceRecentFilesBubbleId = 18;
constexpr int kHoldingSpaceRecentFilesPlaceholderId = 19;
constexpr int kHoldingSpaceScreenCapturePlayIconId = 20;
constexpr int kHoldingSpaceSuggestionsChevronIconId = 21;
constexpr int kHoldingSpaceSuggestionsSectionContainerId = 22;
constexpr int kHoldingSpaceSuggestionsSectionHeaderId = 23;
constexpr int kHoldingSpaceSuggestionsSectionId = 24;
constexpr int kHoldingSpaceTrayDefaultIconId = 25;
constexpr int kHoldingSpaceTrayDropTargetOverlayId = 26;
constexpr int kHoldingSpaceTrayPreviewsIconId = 27;

// The maximum allowed age for files restored into the holding space model.
// Note that this is not enforced for pinned items.
constexpr base::TimeDelta kMaxFileAge = base::Days(1);

// Mime type with wildcard which matches all image types.
constexpr char kMimeTypeImage[] = "image/*";

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CONSTANTS_H_

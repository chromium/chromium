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
constexpr int kHoldingSpaceBubbleWidth = 360;
constexpr gfx::Insets kHoldingSpaceContainerPadding(16);
constexpr int kHoldingSpaceContainerChildSpacing = 16;
constexpr int kHoldingSpaceContainerSpacing = 8;
constexpr gfx::Insets kHoldingSpaceChipPadding(8);
constexpr int kHoldingSpaceChipChildSpacing = 8;
constexpr int kHoldingSpaceChipHeight = 40;
constexpr int kHoldingSpaceChipIconSize = 24;
constexpr int kHoldingSpaceChipWidth = 160;
constexpr int kHoldingSpaceChipsPerRow = 2;
constexpr int kHoldingSpaceColumnSpacing = 8;
constexpr int kHoldingSpaceColumnWidth = 160;
constexpr int kHoldingSpaceContextMenuMargin = 8;
constexpr int kHoldingSpaceCornerRadius = 8;
constexpr int kHoldingSpaceDownloadsChevronIconSize = 20;
constexpr int kHoldingSpaceDownloadsHeaderSpacing = 16;
constexpr int kHoldingSpacePinIconSize = 20;
constexpr int kHoldingSpaceRowSpacing = 8;
constexpr gfx::Insets kHoldingSpaceScreenshotPadding(8);
constexpr int kHoldingSpaceScreenshotSpacing = 8;
constexpr gfx::Size kHoldingSpaceScreenshotSize(104, 80);
constexpr gfx::Insets kHoldingSpaceScreenshotsContainerPadding(8, 0);
constexpr float kHoldingSpaceSelectedOverlayOpacity = 0.24f;
constexpr int kHoldingSpaceTrayMainAxisMargin = 6;

// Context menu commands.
enum HoldingSpaceCommandId {
  kPinItem,
  kCopyImageToClipboard,
  kShowInFolder,
  kUnpinItem,
  kMaxValue = kUnpinItem
};

// View IDs.
constexpr int kHoldingSpacePinnedFilesContainerId = 1;
constexpr int kHoldingSpaceRecentFilesContainerId = 2;

// The maximum allowed age for files restored into the holding space model.
// Note that this is not enforced for pinned items.
constexpr base::TimeDelta kMaxFileAge = base::TimeDelta::FromDays(1);

// The maximum allowed number of downloads to display in holding space UI.
constexpr size_t kMaxDownloads = 2u;

// The maximum allowed number of screenshots to display in holding space UI.
constexpr size_t kMaxScreenshots = 3u;

// Mime type with wildcard which matches all image types.
constexpr char kMimeTypeImage[] = "image/*";

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CONSTANTS_H_

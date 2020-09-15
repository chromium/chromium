// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CONSTANTS_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CONSTANTS_H_

#include "ash/public/cpp/app_menu_constants.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

// Context menu commands.
enum HoldingSpaceCommandId {
  kTogglePinItem,
  kCopyToClipboard,
  kShowInFolder,
  kMaxValue = kShowInFolder
};

// Appearance.
constexpr gfx::Insets kHoldingSpaceContainerPadding(16);
constexpr int kHoldingSpaceContainerSeparation = 8;
constexpr gfx::Insets kHoldingSpaceChipPadding(8);
constexpr int kHoldingSpaceChipChildSpacing = 8;
constexpr int kHoldingSpaceChipCornerRadius = 8;
constexpr int kHoldingSpaceChipIconSize = 24;
constexpr int kHoldingSpaceColumnPadding = 8;
constexpr int kHoldingSpaceColumnWidth = 160;
constexpr int kHoldingSpaceRowPadding = 8;
constexpr gfx::Size kHoldingSpaceScreenshotSize(104, 80);

// View IDs.
constexpr int kHoldingSpacePinnedFilesContainerId = 1;
constexpr int kHoldingSpaceRecentFilesContainerId = 2;

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CONSTANTS_H_

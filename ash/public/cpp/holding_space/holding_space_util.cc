// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_util.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"

namespace ash {
namespace holding_space_util {

gfx::Size GetMaxImageSizeForType(HoldingSpaceItem::Type type) {
  gfx::Size max_size;
  switch (type) {
    case HoldingSpaceItem::Type::kArcDownload:
    case HoldingSpaceItem::Type::kDiagnosticsLog:
    case HoldingSpaceItem::Type::kDownload:
    case HoldingSpaceItem::Type::kLacrosDownload:
    case HoldingSpaceItem::Type::kNearbyShare:
    case HoldingSpaceItem::Type::kPinnedFile:
    case HoldingSpaceItem::Type::kPrintedPdf:
    case HoldingSpaceItem::Type::kScan:
    case HoldingSpaceItem::Type::kPhoneHubCameraRoll:
      max_size =
          gfx::Size(kHoldingSpaceChipIconSize, kHoldingSpaceChipIconSize);
      break;
    case HoldingSpaceItem::Type::kScreenRecording:
    case HoldingSpaceItem::Type::kScreenshot:
      max_size = kHoldingSpaceScreenCaptureSize;
      break;
  }
  // To avoid pixelation, ensure that the holding space image size is at least
  // as large as the default tray icon preview size. The image will be scaled
  // down elsewhere if needed.
  max_size.SetToMax(gfx::Size(kHoldingSpaceTrayIconDefaultPreviewSize,
                              kHoldingSpaceTrayIconDefaultPreviewSize));
  return max_size;
}

bool IsInProgressCommand(HoldingSpaceCommandId command_id) {
  switch (command_id) {
    case HoldingSpaceCommandId::kCancelItem:
    case HoldingSpaceCommandId::kPauseItem:
    case HoldingSpaceCommandId::kResumeItem:
      return true;
    default:
      return false;
  }
}

bool SupportsInProgressCommand(const HoldingSpaceItem* item,
                               HoldingSpaceCommandId command_id) {
  DCHECK(IsInProgressCommand(command_id));
  return std::any_of(
      item->in_progress_commands().begin(), item->in_progress_commands().end(),
      [&](const HoldingSpaceItem::InProgressCommand& in_progress_command) {
        return in_progress_command.command_id == command_id;
      });
}

bool ExecuteInProgressCommand(const HoldingSpaceItem* item,
                              HoldingSpaceCommandId command_id) {
  DCHECK(IsInProgressCommand(command_id));
  for (const auto& in_progress_command : item->in_progress_commands()) {
    if (in_progress_command.command_id == command_id) {
      in_progress_command.handler.Run(item, command_id);
      return true;
    }
  }
  return false;
}

}  // namespace holding_space_util
}  // namespace ash

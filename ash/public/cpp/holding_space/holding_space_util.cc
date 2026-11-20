// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_util.h"

#include <algorithm>

#include "ash/public/cpp/holding_space/holding_space_constants.h"

namespace ash::holding_space_util {

// Utilities -------------------------------------------------------------------

base::flat_set<HoldingSpaceFile::FileSystemType> GetAllFileSystemTypes() {
  return base::flat_set<HoldingSpaceFile::FileSystemType>({
      HoldingSpaceFile::FileSystemType::kArcContent,
      HoldingSpaceFile::FileSystemType::kArcDocumentsProvider,
      HoldingSpaceFile::FileSystemType::kDeviceMedia,
      HoldingSpaceFile::FileSystemType::kDeviceMediaAsFileStorage,
      HoldingSpaceFile::FileSystemType::kDragged,
      HoldingSpaceFile::FileSystemType::kDriveFs,
      HoldingSpaceFile::FileSystemType::kExternal,
      HoldingSpaceFile::FileSystemType::kFuseBox,
      HoldingSpaceFile::FileSystemType::kIsolated,
      HoldingSpaceFile::FileSystemType::kLocal,
      HoldingSpaceFile::FileSystemType::kLocalForPlatformApp,
      HoldingSpaceFile::FileSystemType::kLocalMedia,
      HoldingSpaceFile::FileSystemType::kPersistent,
      HoldingSpaceFile::FileSystemType::kProvided,
      HoldingSpaceFile::FileSystemType::kSmbFs,
      HoldingSpaceFile::FileSystemType::kSyncable,
      HoldingSpaceFile::FileSystemType::kSyncableForInternalSync,
      HoldingSpaceFile::FileSystemType::kTemporary,
      HoldingSpaceFile::FileSystemType::kTest,
      HoldingSpaceFile::FileSystemType::kUnknown,
  });
}

base::flat_set<HoldingSpaceItem::Type> GetAllItemTypes() {
  return base::flat_set<HoldingSpaceItem::Type>({
      HoldingSpaceItem::Type::kArcDownload,
      HoldingSpaceItem::Type::kDiagnosticsLog,
      HoldingSpaceItem::Type::kDownload,
      HoldingSpaceItem::Type::kDriveSuggestion,
      HoldingSpaceItem::Type::kLocalSuggestion,
      HoldingSpaceItem::Type::kNearbyShare,
      HoldingSpaceItem::Type::kPhoneHubCameraRoll,
      HoldingSpaceItem::Type::kPhotoshopWeb,
      HoldingSpaceItem::Type::kPinnedFile,
      HoldingSpaceItem::Type::kPrintedPdf,
      HoldingSpaceItem::Type::kScan,
      HoldingSpaceItem::Type::kScreenRecording,
      HoldingSpaceItem::Type::kScreenRecordingGif,
      HoldingSpaceItem::Type::kScreenshot,
  });
}

gfx::Size GetMaxImageSizeForType(HoldingSpaceItem::Type type) {
  gfx::Size max_size;
  switch (type) {
    case HoldingSpaceItem::Type::kArcDownload:
    case HoldingSpaceItem::Type::kDiagnosticsLog:
    case HoldingSpaceItem::Type::kDownload:
    case HoldingSpaceItem::Type::kDriveSuggestion:
    case HoldingSpaceItem::Type::kLocalSuggestion:
    case HoldingSpaceItem::Type::kNearbyShare:
    case HoldingSpaceItem::Type::kPhoneHubCameraRoll:
    case HoldingSpaceItem::Type::kPhotoshopWeb:
    case HoldingSpaceItem::Type::kPinnedFile:
    case HoldingSpaceItem::Type::kPrintedPdf:
    case HoldingSpaceItem::Type::kScan:
      max_size =
          gfx::Size(kHoldingSpaceChipIconSize, kHoldingSpaceChipIconSize);
      break;
    case HoldingSpaceItem::Type::kScreenRecording:
    case HoldingSpaceItem::Type::kScreenRecordingGif:
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
    case HoldingSpaceCommandId::kOpenItem:
    case HoldingSpaceCommandId::kPauseItem:
    case HoldingSpaceCommandId::kResumeItem:
    case HoldingSpaceCommandId::kViewItemDetailsInBrowser:
      return true;
    default:
      return false;
  }
}

bool SupportsInProgressCommand(const HoldingSpaceItem* item,
                               HoldingSpaceCommandId command_id) {
  DCHECK(IsInProgressCommand(command_id));
  return std::ranges::contains(
      item->in_progress_commands(), command_id,
      &HoldingSpaceItem::InProgressCommand::command_id);
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

// NOTE: These values are persisted to histograms and must remain unchanged.
std::string_view ToString(HoldingSpaceFile::FileSystemType type) {
  switch (type) {
    case HoldingSpaceFile::FileSystemType::kArcContent:
      return "ArcContent";
    case HoldingSpaceFile::FileSystemType::kArcDocumentsProvider:
      return "ArcDocumentsProvider";
    case HoldingSpaceFile::FileSystemType::kDeviceMedia:
      return "DeviceMedia";
    case HoldingSpaceFile::FileSystemType::kDeviceMediaAsFileStorage:
      return "DeviceMediaAsFileStorage";
    case HoldingSpaceFile::FileSystemType::kDragged:
      return "Dragged";
    case HoldingSpaceFile::FileSystemType::kDriveFs:
      return "DriveFs";
    case HoldingSpaceFile::FileSystemType::kExternal:
      return "External";
    case HoldingSpaceFile::FileSystemType::kFuseBox:
      return "FuseBox";
    case HoldingSpaceFile::FileSystemType::kIsolated:
      return "Isolated";
    case HoldingSpaceFile::FileSystemType::kLocal:
      return "Local";
    case HoldingSpaceFile::FileSystemType::kLocalForPlatformApp:
      return "LocalForPlatformApp";
    case HoldingSpaceFile::FileSystemType::kLocalMedia:
      return "LocalMedia";
    case HoldingSpaceFile::FileSystemType::kPersistent:
      return "Persistent";
    case HoldingSpaceFile::FileSystemType::kProvided:
      return "Provided";
    case HoldingSpaceFile::FileSystemType::kSmbFs:
      return "SmbFs";
    case HoldingSpaceFile::FileSystemType::kSyncable:
      return "Syncable";
    case HoldingSpaceFile::FileSystemType::kSyncableForInternalSync:
      return "SyncableForInternalSync";
    case HoldingSpaceFile::FileSystemType::kTemporary:
      return "Temporary";
    case HoldingSpaceFile::FileSystemType::kTest:
      return "Test";
    case HoldingSpaceFile::FileSystemType::kUnknown:
      return "Unknown";
  }
}

// NOTE: These values are persisted to histograms and must remain unchanged.
std::string_view ToString(HoldingSpaceItem::Type type) {
  switch (type) {
    case HoldingSpaceItem::Type::kArcDownload:
      return "ArcDownload";
    case HoldingSpaceItem::Type::kDiagnosticsLog:
      return "DiagnosticsLog";
    case HoldingSpaceItem::Type::kDownload:
      return "Download";
    case HoldingSpaceItem::Type::kDriveSuggestion:
      return "DriveSuggestion";
    case HoldingSpaceItem::Type::kLocalSuggestion:
      return "LocalSuggestion";
    case HoldingSpaceItem::Type::kNearbyShare:
      return "NearbyShare";
    case HoldingSpaceItem::Type::kPhoneHubCameraRoll:
      return "PhoneHubCameraRoll";
    case HoldingSpaceItem::Type::kPhotoshopWeb:
      return "PhotoshopWeb";
    case HoldingSpaceItem::Type::kPinnedFile:
      return "PinnedFile";
    case HoldingSpaceItem::Type::kPrintedPdf:
      return "PrintedPdf";
    case HoldingSpaceItem::Type::kScan:
      return "Scan";
    case HoldingSpaceItem::Type::kScreenRecording:
      return "ScreenRecording";
    case HoldingSpaceItem::Type::kScreenRecordingGif:
      return "ScreenRecordingGif";
    case HoldingSpaceItem::Type::kScreenshot:
      return "Screenshot";
  }
}

}  // namespace ash::holding_space_util

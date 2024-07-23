// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_util.h"

#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/pickle.h"
#include "base/strings/string_split.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "url/gurl.h"

namespace ash::holding_space_util {
namespace {

// Helpers ---------------------------------------------------------------------

// Returns the file paths extracted from the specified `data` at the filenames
// storage location.
std::vector<base::FilePath> ExtractFilePathsFromFilenames(
    const ui::OSExchangeData& data) {
  std::vector<base::FilePath> paths;

  if (!data.HasFile()) {
    return paths;
  }

  std::optional<std::vector<ui::FileInfo>> filenames = data.GetFilenames();
  if (!filenames.has_value()) {
    return paths;
  }

  return base::ToVector(filenames.value(), &ui::FileInfo::path);
}

// TODO(http://b/279031685): Ask Files app team to own a util API for this.
// Returns the file paths extracted from the specified `data` at the file
// system sources storage location used by the Files app.
std::vector<base::FilePath> ExtractFilePathsFromFileSystemSources(
    const ui::OSExchangeData& data) {
  std::vector<base::FilePath> paths;

  std::optional<base::Pickle> pickle =
      data.GetPickledData(ui::ClipboardFormatType::DataTransferCustomType());
  if (!pickle.has_value()) {
    return paths;
  }

  std::optional<std::u16string> maybe_sources =
      ui::ReadCustomDataForType(pickle.value(), u"fs/sources");
  if (!maybe_sources.has_value()) {
    return paths;
  }

  const HoldingSpaceClient* client = HoldingSpaceController::Get()->client();
  if (!client) {
    return paths;
  }

  for (const auto& source :
       base::SplitStringPiece(*maybe_sources, u"\n", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    if (auto path = client->CrackFileSystemUrl(GURL(source)); !path.empty()) {
      paths.emplace_back(std::move(path));
    }
  }

  return paths;
}

}  // namespace

// Utilities -------------------------------------------------------------------

std::vector<base::FilePath> ExtractFilePaths(const ui::OSExchangeData& data,
                                             bool fallback_to_filenames) {
  // Prefer extracting file paths from file system sources when possible. The
  // Files app populates both file system sources and filenames storage
  // locations, but only the former contains directory paths.
  auto paths = ExtractFilePathsFromFileSystemSources(data);
  return (paths.empty() && fallback_to_filenames)
             ? ExtractFilePathsFromFilenames(data)
             : paths;
}

base::flat_set<HoldingSpaceFile::FileSystemType> GetAllFileSystemTypes() {
  return base::flat_set<HoldingSpaceFile::FileSystemType>({
      HoldingSpaceFile::FileSystemType::kArcContent,
      HoldingSpaceFile::FileSystemType::kArcDocumentsProvider,
      HoldingSpaceFile::FileSystemType::kDeviceMedia,
      HoldingSpaceFile::FileSystemType::kDeviceMediaAsFileStorage,
      HoldingSpaceFile::FileSystemType::kDragged,
      HoldingSpaceFile::FileSystemType::kDriveFs,
      HoldingSpaceFile::FileSystemType::kExternal,
      HoldingSpaceFile::FileSystemType::kForTransientFile,
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
      HoldingSpaceItem::Type::kLacrosDownload,
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
    case HoldingSpaceItem::Type::kLacrosDownload:
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
  return base::Contains(item->in_progress_commands(), command_id,
                        &HoldingSpaceItem::InProgressCommand::command_id);
}

bool ExecuteInProgressCommand(const HoldingSpaceItem* item,
                              HoldingSpaceCommandId command_id,
                              holding_space_metrics::EventSource event_source) {
  DCHECK(IsInProgressCommand(command_id));
  for (const auto& in_progress_command : item->in_progress_commands()) {
    if (in_progress_command.command_id == command_id) {
      in_progress_command.handler.Run(item, command_id, event_source);
      return true;
    }
  }
  return false;
}

// NOTE: These values are persisted to histograms and must remain unchanged.
std::string ToString(HoldingSpaceFile::FileSystemType type) {
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
    case HoldingSpaceFile::FileSystemType::kForTransientFile:
      return "ForTransientFile";
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
std::string ToString(HoldingSpaceItem::Type type) {
  switch (type) {
    case HoldingSpaceItem::Type::kArcDownload:
      return "ArcDownload";
    case HoldingSpaceItem::Type::kDiagnosticsLog:
      return "DiagnosticsLog";
    case HoldingSpaceItem::Type::kDownload:
      return "Download";
    case HoldingSpaceItem::Type::kDriveSuggestion:
      return "DriveSuggestion";
    case HoldingSpaceItem::Type::kLacrosDownload:
      return "LacrosDownload";
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

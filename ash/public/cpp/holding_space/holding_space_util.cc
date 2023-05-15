// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_util.h"

#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "base/containers/contains.h"
#include "base/pickle.h"
#include "base/strings/string_split.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
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

  std::vector<ui::FileInfo> filenames;
  if (!data.GetFilenames(&filenames)) {
    return paths;
  }

  for (const ui::FileInfo& filename : filenames) {
    paths.emplace_back(filename.path);
  }

  return paths;
}

// TODO(http://b/279031685): Ask Files app team to own a util API for this.
// Returns the file paths extracted from the specified `data` at the file
// system sources storage location used by the Files app.
std::vector<base::FilePath> ExtractFilePathsFromFileSystemSources(
    const ui::OSExchangeData& data) {
  std::vector<base::FilePath> paths;

  base::Pickle p;
  if (!data.GetPickledData(ui::ClipboardFormatType::WebCustomDataType(), &p)) {
    return paths;
  }

  std::u16string sources;
  ui::ReadCustomDataForType(p.data(), p.size(), u"fs/sources", &sources);
  if (sources.empty()) {
    return paths;
  }

  const HoldingSpaceClient* client = HoldingSpaceController::Get()->client();
  if (!client) {
    return paths;
  }

  for (const auto& source : base::SplitStringPiece(
           sources, u"\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
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

gfx::Size GetMaxImageSizeForType(HoldingSpaceItem::Type type) {
  gfx::Size max_size;
  switch (type) {
    case HoldingSpaceItem::Type::kArcDownload:
    case HoldingSpaceItem::Type::kCameraAppPhoto:
    case HoldingSpaceItem::Type::kCameraAppScanJpg:
    case HoldingSpaceItem::Type::kCameraAppScanPdf:
    case HoldingSpaceItem::Type::kCameraAppVideoGif:
    case HoldingSpaceItem::Type::kCameraAppVideoMp4:
    case HoldingSpaceItem::Type::kDiagnosticsLog:
    case HoldingSpaceItem::Type::kDownload:
    case HoldingSpaceItem::Type::kDriveSuggestion:
    case HoldingSpaceItem::Type::kLacrosDownload:
    case HoldingSpaceItem::Type::kLocalSuggestion:
    case HoldingSpaceItem::Type::kNearbyShare:
    case HoldingSpaceItem::Type::kPhoneHubCameraRoll:
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
  return base::Contains(item->in_progress_commands(), command_id,
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
std::string ToString(HoldingSpaceItem::Type type) {
  switch (type) {
    case HoldingSpaceItem::Type::kArcDownload:
      return "ArcDownload";
    case HoldingSpaceItem::Type::kCameraAppPhoto:
      return "CameraAppPhoto";
    case HoldingSpaceItem::Type::kCameraAppScanJpg:
      return "CameraAppScanJpg";
    case HoldingSpaceItem::Type::kCameraAppScanPdf:
      return "CameraAppScanPdf";
    case HoldingSpaceItem::Type::kCameraAppVideoGif:
      return "CameraAppVideoGif";
    case HoldingSpaceItem::Type::kCameraAppVideoMp4:
      return "CameraAppVideoMp4";
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

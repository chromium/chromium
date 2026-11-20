// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_util.h"

#include <string>

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/containers/enum_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::holding_space_util {
namespace {

// Helpers ---------------------------------------------------------------------

// Returns an enum set for the specified type containing all values between
// `kMinValue` and `kMaxValue`. NOTE: Some values contained in the set may be
// undefined if type is not contiguous.
template <typename T>
base::EnumSet<T> CreateEnumSet() {
  return base::EnumSet<T>::All();
}

}  // namespace

// Tests -----------------------------------------------------------------------

using HoldingSpaceUtilTest = ::testing::Test;

// Verifies that `GetAllFileSystemTypes()` contains every value defined in
// `HoldingSpaceFile::FileSystemType` and no undefined values. This differs from
// `base::EnumSet<HoldingSpaceFile::FileSystemType, ...>::All()` which contains
// undefined values if the underlying enum is not contiguous within its range.
TEST_F(HoldingSpaceUtilTest, GetAllFileSystemTypes) {
  const base::flat_set<HoldingSpaceFile::FileSystemType> all_types =
      GetAllFileSystemTypes();

  for (const auto type : CreateEnumSet<HoldingSpaceFile::FileSystemType>()) {
    bool should_exist_in_all_types_set = false;

    switch (type) {
      case HoldingSpaceFile::FileSystemType::kArcContent:
      case HoldingSpaceFile::FileSystemType::kArcDocumentsProvider:
      case HoldingSpaceFile::FileSystemType::kDeviceMedia:
      case HoldingSpaceFile::FileSystemType::kDeviceMediaAsFileStorage:
      case HoldingSpaceFile::FileSystemType::kDragged:
      case HoldingSpaceFile::FileSystemType::kDriveFs:
      case HoldingSpaceFile::FileSystemType::kExternal:
      case HoldingSpaceFile::FileSystemType::kFuseBox:
      case HoldingSpaceFile::FileSystemType::kIsolated:
      case HoldingSpaceFile::FileSystemType::kLocal:
      case HoldingSpaceFile::FileSystemType::kLocalForPlatformApp:
      case HoldingSpaceFile::FileSystemType::kLocalMedia:
      case HoldingSpaceFile::FileSystemType::kPersistent:
      case HoldingSpaceFile::FileSystemType::kProvided:
      case HoldingSpaceFile::FileSystemType::kSmbFs:
      case HoldingSpaceFile::FileSystemType::kSyncable:
      case HoldingSpaceFile::FileSystemType::kSyncableForInternalSync:
      case HoldingSpaceFile::FileSystemType::kTemporary:
      case HoldingSpaceFile::FileSystemType::kTest:
      case HoldingSpaceFile::FileSystemType::kUnknown:
        should_exist_in_all_types_set = true;
    }

    EXPECT_EQ(all_types.contains(type), should_exist_in_all_types_set);
  }
}

// Verifies that `GetAllItemTypes()` contains every value defined in
// `HoldingSpaceItem::Type` and no undefined values. This differs from
// `base::EnumSet<HoldingSpaceItem::Type, ...>::All()` which contains undefined
// values if the underlying enum is not contiguous within its range.
TEST_F(HoldingSpaceUtilTest, GetAllItemTypes) {
  const base::flat_set<HoldingSpaceItem::Type> all_types = GetAllItemTypes();

  for (const auto type : CreateEnumSet<HoldingSpaceItem::Type>()) {
    bool should_exist_in_all_types_set = false;

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
      case HoldingSpaceItem::Type::kScreenRecording:
      case HoldingSpaceItem::Type::kScreenRecordingGif:
      case HoldingSpaceItem::Type::kScreenshot:
        should_exist_in_all_types_set = true;
    }

    EXPECT_EQ(all_types.contains(type), should_exist_in_all_types_set);
  }
}

// Verifies that `ToString(HoldingSpaceFile::FileSystemType)` is WAI.
// NOTE: These values are persisted to histograms and must remain unchanged.
TEST_F(HoldingSpaceUtilTest, FileSystemTypeToString) {
  for (const auto fs_type : GetAllFileSystemTypes()) {
    std::string expected_string;
    switch (fs_type) {
      case HoldingSpaceFile::FileSystemType::kArcContent:
        expected_string = "ArcContent";
        break;
      case HoldingSpaceFile::FileSystemType::kArcDocumentsProvider:
        expected_string = "ArcDocumentsProvider";
        break;
      case HoldingSpaceFile::FileSystemType::kDeviceMedia:
        expected_string = "DeviceMedia";
        break;
      case HoldingSpaceFile::FileSystemType::kDeviceMediaAsFileStorage:
        expected_string = "DeviceMediaAsFileStorage";
        break;
      case HoldingSpaceFile::FileSystemType::kDragged:
        expected_string = "Dragged";
        break;
      case HoldingSpaceFile::FileSystemType::kDriveFs:
        expected_string = "DriveFs";
        break;
      case HoldingSpaceFile::FileSystemType::kExternal:
        expected_string = "External";
        break;
      case HoldingSpaceFile::FileSystemType::kFuseBox:
        expected_string = "FuseBox";
        break;
      case HoldingSpaceFile::FileSystemType::kIsolated:
        expected_string = "Isolated";
        break;
      case HoldingSpaceFile::FileSystemType::kLocal:
        expected_string = "Local";
        break;
      case HoldingSpaceFile::FileSystemType::kLocalForPlatformApp:
        expected_string = "LocalForPlatformApp";
        break;
      case HoldingSpaceFile::FileSystemType::kLocalMedia:
        expected_string = "LocalMedia";
        break;
      case HoldingSpaceFile::FileSystemType::kPersistent:
        expected_string = "Persistent";
        break;
      case HoldingSpaceFile::FileSystemType::kProvided:
        expected_string = "Provided";
        break;
      case HoldingSpaceFile::FileSystemType::kSmbFs:
        expected_string = "SmbFs";
        break;
      case HoldingSpaceFile::FileSystemType::kSyncable:
        expected_string = "Syncable";
        break;
      case HoldingSpaceFile::FileSystemType::kSyncableForInternalSync:
        expected_string = "SyncableForInternalSync";
        break;
      case HoldingSpaceFile::FileSystemType::kTemporary:
        expected_string = "Temporary";
        break;
      case HoldingSpaceFile::FileSystemType::kTest:
        expected_string = "Test";
        break;
      case HoldingSpaceFile::FileSystemType::kUnknown:
        expected_string = "Unknown";
        break;
    }
    EXPECT_EQ(ToString(fs_type), expected_string);
  }
}

// Verifies that `ToString(HoldingSpaceItem::Type)` is WAI.
// NOTE: These values are persisted to histograms and must remain unchanged.
TEST_F(HoldingSpaceUtilTest, ItemTypeToString) {
  for (const auto type : GetAllItemTypes()) {
    std::string expected_string;
    switch (type) {
      case HoldingSpaceItem::Type::kArcDownload:
        expected_string = "ArcDownload";
        break;
      case HoldingSpaceItem::Type::kDiagnosticsLog:
        expected_string = "DiagnosticsLog";
        break;
      case HoldingSpaceItem::Type::kDownload:
        expected_string = "Download";
        break;
      case HoldingSpaceItem::Type::kDriveSuggestion:
        expected_string = "DriveSuggestion";
        break;
      case HoldingSpaceItem::Type::kLocalSuggestion:
        expected_string = "LocalSuggestion";
        break;
      case HoldingSpaceItem::Type::kNearbyShare:
        expected_string = "NearbyShare";
        break;
      case HoldingSpaceItem::Type::kPhoneHubCameraRoll:
        expected_string = "PhoneHubCameraRoll";
        break;
      case HoldingSpaceItem::Type::kPhotoshopWeb:
        expected_string = "PhotoshopWeb";
        break;
      case HoldingSpaceItem::Type::kPinnedFile:
        expected_string = "PinnedFile";
        break;
      case HoldingSpaceItem::Type::kPrintedPdf:
        expected_string = "PrintedPdf";
        break;
      case HoldingSpaceItem::Type::kScan:
        expected_string = "Scan";
        break;
      case HoldingSpaceItem::Type::kScreenRecording:
        expected_string = "ScreenRecording";
        break;
      case HoldingSpaceItem::Type::kScreenRecordingGif:
        expected_string = "ScreenRecordingGif";
        break;
      case HoldingSpaceItem::Type::kScreenshot:
        expected_string = "Screenshot";
        break;
    }
    EXPECT_EQ(ToString(type), expected_string);
  }
}

}  // namespace ash::holding_space_util

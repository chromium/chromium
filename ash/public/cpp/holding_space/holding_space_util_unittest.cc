// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_util.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/mock_holding_space_client.h"
#include "ash/test/ash_test_base.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_non_backed.h"

namespace ash::holding_space_util {
namespace {

// Helpers ---------------------------------------------------------------------

// Returns a created `ui::OSExchangeData` instance with optional file paths
// stored (a) at the file system sources storage location used by the Files app,
// and/or (b) at the filenames storage location.
std::unique_ptr<ui::OSExchangeData> CreateOSExchangeData(
    const std::u16string& file_system_sources,
    const std::vector<base::FilePath>& filenames) {
  auto data = std::make_unique<ui::OSExchangeData>(
      std::make_unique<ui::OSExchangeDataProviderNonBacked>());

  if (!file_system_sources.empty()) {
    base::Pickle pickle;
    ui::WriteCustomDataToPickle(
        std::unordered_map<std::u16string, std::u16string>(
            {{u"fs/sources", file_system_sources}}),
        &pickle);
    data->SetPickledData(ui::ClipboardFormatType::WebCustomDataType(), pickle);
  }

  if (!filenames.empty()) {
    std::vector<ui::FileInfo> infos;
    for (const auto& filename : filenames) {
      infos.emplace_back(filename, filename.BaseName());
    }
    data->SetFilenames(infos);
  }

  return data;
}

}  // namespace

// Tests -----------------------------------------------------------------------

using HoldingSpaceUtilTest = NoSessionAshTestBase;

// Verifies that `holding_space_util::ExtractFilePaths()` is WAI.
TEST_F(HoldingSpaceUtilTest, ExtractFilePaths) {
  std::unique_ptr<ui::OSExchangeData> data;
  std::u16string file_system_sources;
  std::vector<base::FilePath> filenames;

  // Log in a user.
  AccountId account_id = AccountId::FromUserEmail("user@test");
  SimulateUserLogin(account_id);

  // Initialize holding space for the user.
  HoldingSpaceModel model;
  testing::NiceMock<MockHoldingSpaceClient> client;
  HoldingSpaceController* controller = HoldingSpaceController::Get();
  controller->RegisterClientAndModelForUser(account_id, &client, &model);

  // Configure the `client` to crack file system URLs.
  ON_CALL(client, CrackFileSystemUrl)
      .WillByDefault(testing::Invoke([](const GURL& file_system_url) {
        return base::FilePath(base::StrCat(
            {"//path/to/", std::string(&file_system_url.spec().back())}));
      }));

  // Case: Empty.
  data = CreateOSExchangeData(file_system_sources, filenames);
  for (bool fallback_to_filenames : {false, true}) {
    EXPECT_TRUE(ExtractFilePaths(*data, fallback_to_filenames).empty());
  }

  // Case: Only file system sources.
  file_system_sources = u"file-system:a\nfile-system:b\nfile-system:c";
  data = CreateOSExchangeData(file_system_sources, filenames);
  for (bool fallback_to_filenames : {false, true}) {
    EXPECT_THAT(
        ExtractFilePaths(*data, fallback_to_filenames),
        testing::ElementsAre(testing::Eq(base::FilePath("//path/to/a")),
                             testing::Eq(base::FilePath("//path/to/b")),
                             testing::Eq(base::FilePath("//path/to/c"))));
  }

  // Case: Both file system sources and filenames.
  filenames.emplace_back("//path/to/d");
  filenames.emplace_back("//path/to/e");
  filenames.emplace_back("//path/to/f");
  data = CreateOSExchangeData(file_system_sources, filenames);
  for (bool fallback_to_filenames : {false, true}) {
    EXPECT_THAT(
        ExtractFilePaths(*data, fallback_to_filenames),
        testing::ElementsAre(testing::Eq(base::FilePath("//path/to/a")),
                             testing::Eq(base::FilePath("//path/to/b")),
                             testing::Eq(base::FilePath("//path/to/c"))));
  }

  // Case: Only filenames.
  file_system_sources.clear();
  data = CreateOSExchangeData(file_system_sources, filenames);
  for (bool fallback_to_filenames : {false, true}) {
    EXPECT_THAT(
        ExtractFilePaths(*data, fallback_to_filenames),
        testing::Conditional(
            fallback_to_filenames,
            testing::ElementsAre(testing::Eq(base::FilePath("//path/to/d")),
                                 testing::Eq(base::FilePath("//path/to/e")),
                                 testing::Eq(base::FilePath("//path/to/f"))),
            testing::IsEmpty()));
  }

  // Clean up holding space for the user before client/model go out of scope.
  controller->RegisterClientAndModelForUser(account_id, /*client=*/nullptr,
                                            /*model=*/nullptr);
}

// Verifies that `holding_space_util::ToString()` is WAI.
// NOTE: These values are persisted to histograms and must remain unchanged.
TEST_F(HoldingSpaceUtilTest, ToString) {
  for (size_t i = 0; i < static_cast<size_t>(HoldingSpaceItem::Type::kMaxValue);
       ++i) {
    auto type = static_cast<HoldingSpaceItem::Type>(i);
    std::string expected_string;
    switch (type) {
      case HoldingSpaceItem::Type::kArcDownload:
        expected_string = "ArcDownload";
        break;
      case HoldingSpaceItem::Type::kCameraAppPhoto:
        expected_string = "CameraAppPhoto";
        break;
      case HoldingSpaceItem::Type::kCameraAppScanJpg:
        expected_string = "CameraAppScanJpg";
        break;
      case HoldingSpaceItem::Type::kCameraAppScanPdf:
        expected_string = "CameraAppScanPdf";
        break;
      case HoldingSpaceItem::Type::kCameraAppVideoGif:
        expected_string = "CameraAppVideoGif";
        break;
      case HoldingSpaceItem::Type::kCameraAppVideoMp4:
        expected_string = "CameraAppVideoMp4";
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
      case HoldingSpaceItem::Type::kLacrosDownload:
        expected_string = "LacrosDownload";
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

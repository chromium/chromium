// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_util.h"

#include <string>

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace holding_space_util {

using HoldingSpaceUtilTest = ::testing::Test;

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
      case HoldingSpaceItem::Type::kScreenshot:
        expected_string = "Screenshot";
        break;
    }
    EXPECT_EQ(ToString(type), expected_string);
  }
}

}  // namespace holding_space_util
}  // namespace ash

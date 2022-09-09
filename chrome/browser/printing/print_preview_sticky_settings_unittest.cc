// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_sticky_settings.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

class PrintPreviewStickySettingsUnittest : public testing::Test {
 public:
  PrintPreviewStickySettingsUnittest() = default;
  ~PrintPreviewStickySettingsUnittest() override = default;

  PrintPreviewStickySettingsUnittest(
      const PrintPreviewStickySettingsUnittest&) = delete;
  PrintPreviewStickySettingsUnittest& operator=(
      const PrintPreviewStickySettingsUnittest&) = delete;

 protected:
  PrintPreviewStickySettings sticky_settings_;
};

TEST_F(PrintPreviewStickySettingsUnittest, GetPrinterRecentlyUsed) {
  const std::string kRecentlyUsedRanksStr = R"({
    "version": 2,
    "recentDestinations": [
      {
        "id":"id1",
        "capabilities": {}
      },
      {
        "id": "id2",
        "origin": "chrome_os"
      }
    ]
  })";
  sticky_settings_.StoreAppState(kRecentlyUsedRanksStr);
  const base::flat_map<std::string, int> kExpectedRecentlyUsedRanks(
      {{"id1", 0}, {"id2", 1}});
  EXPECT_EQ(kExpectedRecentlyUsedRanks,
            sticky_settings_.GetPrinterRecentlyUsedRanks());
  const std::vector<std::string> kExpectedRecentlyUsedPrinters({"id1", "id2"});
  EXPECT_EQ(kExpectedRecentlyUsedPrinters,
            sticky_settings_.GetRecentlyUsedPrinters());
}

TEST_F(PrintPreviewStickySettingsUnittest,
       GetPrinterRecentlyUsed_NoRecentDestinations) {
  const std::string kRecentlyUsedRanksStr = R"({"version": 2})";
  sticky_settings_.StoreAppState(kRecentlyUsedRanksStr);
  EXPECT_TRUE(sticky_settings_.GetPrinterRecentlyUsedRanks().empty());
  EXPECT_TRUE(sticky_settings_.GetRecentlyUsedPrinters().empty());
}

}  // namespace printing

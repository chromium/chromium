// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_util.h"

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/borealis/borealis_window_manager_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/test_screen.h"

namespace borealis {
namespace {

class BorealisUtilTest : public testing::Test {
 public:
  BorealisUtilTest() { display::Screen::SetScreenInstance(&test_screen_); }
  ~BorealisUtilTest() override { display::Screen::SetScreenInstance(nullptr); }

 protected:
  display::test::TestScreen test_screen_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BorealisUtilTest, GetBorealisAppIdReturnsEmptyOnFailure) {
  EXPECT_EQ(GetBorealisAppId("foo"), absl::nullopt);
}

TEST_F(BorealisUtilTest, GetBorealisAppIdReturnsId) {
  EXPECT_EQ(GetBorealisAppId("steam://rungameid/123").value(), 123);
}

TEST_F(BorealisUtilTest, GetBorealisAppIdFromWindowReturnsEmptyOnFailure) {
  std::unique_ptr<aura::Window> window =
      MakeWindow("org.chromium.guest_os.borealis.wmclass.foo");
  EXPECT_EQ(GetBorealisAppId(window.get()), absl::nullopt);
}

TEST_F(BorealisUtilTest, GetBorealisAppIdFromWindowReturnsId) {
  std::unique_ptr<aura::Window> window =
      MakeWindow("org.chromium.guest_os.borealis.xprop.123");
  EXPECT_EQ(GetBorealisAppId(window.get()).value(), 123);
}

TEST_F(BorealisUtilTest, IsNonGameBorealisAppReturnsTrueForNonGameBorealisApp) {
  EXPECT_TRUE(IsNonGameBorealisApp(
      "borealis_anon:org.chromium.guest_os.borealis.xid.100"));
}

TEST_F(BorealisUtilTest, IsNonGameBorealisAppReturnsFalseForGames) {
  EXPECT_FALSE(
      IsNonGameBorealisApp("borealis_anon:org.chromium.guest_os.borealis.app"));
}

TEST_F(BorealisUtilTest, ProtonTitleUnknownBorealisAppId) {
  absl::optional<int> game_id;
  std::string output =
      "GameID: 123, Proton: Proton 1.2, SLR: SLR - Name, "
      "Timestamp: 2021-01-01 00:00:00";
  borealis::CompatToolInfo info =
      borealis::ParseCompatToolInfo(game_id, output);
  EXPECT_TRUE(info.game_id.has_value());
  EXPECT_EQ(info.game_id.value(), 123);
  EXPECT_EQ(info.proton, "Proton 1.2");
  EXPECT_EQ(info.slr, "SLR - Name");
}

TEST_F(BorealisUtilTest, ProtonTitleKnownBorealisAppId) {
  absl::optional<int> game_id = 123;
  std::string output =
      "GameID: 123, Proton: Proton 1.2, SLR: SLR - Name, "
      "Timestamp: 2021-01-01 00:00:00";
  borealis::CompatToolInfo info =
      borealis::ParseCompatToolInfo(game_id, output);
  EXPECT_TRUE(info.game_id.has_value());
  EXPECT_EQ(info.game_id.value(), 123);
  EXPECT_EQ(info.proton, "Proton 1.2");
  EXPECT_EQ(info.slr, "SLR - Name");
}

TEST_F(BorealisUtilTest, ProtonTitleMultiLineUnknownBorealisAppId) {
  absl::optional<int> game_id;
  std::string output =
      "GameID: 123, Proton: Proton 1.2, SLR: SLR - Name, "
      "Timestamp: 2021-01-01 00:00:00\n"
      "GameID: 456, Proton: Proton 4.5, SLR: SLR - Name2, "
      "Timestamp: 2021-01-01 00:00:00";
  borealis::CompatToolInfo info =
      borealis::ParseCompatToolInfo(game_id, output);
  EXPECT_TRUE(info.game_id.has_value());
  EXPECT_EQ(info.game_id.value(), 123);
  EXPECT_EQ(info.proton, "Proton 1.2");
  EXPECT_EQ(info.slr, "SLR - Name");
}

TEST_F(BorealisUtilTest, ProtonTitleMultiLineKnownBorealisAppId) {
  absl::optional<int> game_id = 123;
  std::string output =
      "GameID: 123, Proton: Proton 1.2, SLR: SLR - Name, "
      "Timestamp: 2021-01-01 00:00:00\n"
      "GameID: 456, Proton: Proton 4.5, SLR: SLR - Name2, "
      "Timestamp: 2021-01-01 00:00:00";
  borealis::CompatToolInfo info =
      borealis::ParseCompatToolInfo(game_id, output);
  EXPECT_TRUE(info.game_id.has_value());
  EXPECT_EQ(info.game_id.value(), 123);
  EXPECT_EQ(info.proton, "Proton 1.2");
  EXPECT_EQ(info.slr, "SLR - Name");
}

TEST_F(BorealisUtilTest, ProtonTitleGameIdMismatch) {
  absl::optional<int> game_id = 123;
  std::string output =
      "GameID: 456, Proton: Proton 1.2, SLR: SLR - Name, "
      "Timestamp: 2021-01-01 00:00:00";
  borealis::CompatToolInfo info =
      borealis::ParseCompatToolInfo(game_id, output);
  EXPECT_TRUE(info.game_id.has_value());
  EXPECT_EQ(info.game_id, 456);
  EXPECT_EQ(info.proton, borealis::kCompatToolVersionGameMismatch);
  EXPECT_EQ(info.slr, borealis::kCompatToolVersionGameMismatch);
}

TEST_F(BorealisUtilTest, SLRTitleUnknownBorealisAppId) {
  absl::optional<int> game_id;
  std::string output =
      "GameID: 123, Proton: None, SLR: SLR - Name, "
      "Timestamp: 2021-01-01 00:00:00";
  borealis::CompatToolInfo info =
      borealis::ParseCompatToolInfo(game_id, output);
  EXPECT_TRUE(info.game_id.has_value());
  EXPECT_EQ(info.game_id.value(), 123);
  EXPECT_EQ(info.proton, "None");
  EXPECT_EQ(info.slr, "SLR - Name");
}

TEST_F(BorealisUtilTest, SLRTitleKnownBorealisAppId) {
  absl::optional<int> game_id = 123;
  std::string output =
      "GameID: 123, Proton: None, SLR: SLR - Name, "
      "Timestamp: 2021-01-01 00:00:00";
  borealis::CompatToolInfo info =
      borealis::ParseCompatToolInfo(game_id, output);
  EXPECT_TRUE(info.game_id.has_value());
  EXPECT_EQ(info.game_id.value(), 123);
  EXPECT_EQ(info.proton, "None");
  EXPECT_EQ(info.slr, "SLR - Name");
}

TEST_F(BorealisUtilTest, SLRTitleGameIdMismatch) {
  absl::optional<int> game_id = 123;
  std::string output =
      "GameID: 456, Proton: None, SLR: SLR - Name, "
      "Timestamp: 2021-01-01 00:00:00";
  borealis::CompatToolInfo info =
      borealis::ParseCompatToolInfo(game_id, output);
  EXPECT_TRUE(info.game_id.has_value());
  EXPECT_EQ(info.game_id, 456);
  EXPECT_EQ(info.proton, borealis::kCompatToolVersionGameMismatch);
  EXPECT_EQ(info.slr, borealis::kCompatToolVersionGameMismatch);
}

TEST_F(BorealisUtilTest, LinuxTitleUnknownBorealisAppId) {
  absl::optional<int> game_id;
  std::string output =
      "GameID: 123, Proton: None, SLR: None, "
      "Timestamp: 2021-01-01 00:00:00";
  borealis::CompatToolInfo info =
      borealis::ParseCompatToolInfo(game_id, output);
  EXPECT_TRUE(info.game_id.has_value());
  EXPECT_EQ(info.game_id, 123);
  EXPECT_EQ(info.proton, "None");
  EXPECT_EQ(info.slr, "None");
}

TEST_F(BorealisUtilTest, LinuxTitleKnownBorealisAppId) {
  absl::optional<int> game_id = 123;
  std::string output =
      "GameID: 123, Proton: None, SLR: None, "
      "Timestamp: 2021-01-01 00:00:00";
  borealis::CompatToolInfo info =
      borealis::ParseCompatToolInfo(game_id, output);
  EXPECT_TRUE(info.game_id.has_value());
  EXPECT_EQ(info.game_id, 123);
  EXPECT_EQ(info.proton, "None");
  EXPECT_EQ(info.slr, "None");
}

TEST_F(BorealisUtilTest, LinuxTitleAfterProtonTitle) {
  absl::optional<int> game_id;
  std::string output =
      "GameID: 123, Proton: None, SLR: None, "
      "Timestamp: 2021-01-01 00:00:00\n"
      "GameID: 456, Proton: Proton 4.5, SLR: SLR - Name, "
      "Timestamp: 2021-01-01 00:00:00";
  borealis::CompatToolInfo info =
      borealis::ParseCompatToolInfo(game_id, output);
  EXPECT_TRUE(info.game_id.has_value());
  EXPECT_EQ(info.game_id, 123);
  EXPECT_EQ(info.proton, "None");
  EXPECT_EQ(info.slr, "None");
}

}  // namespace
}  // namespace borealis

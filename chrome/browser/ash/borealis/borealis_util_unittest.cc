// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_util.h"

#include <string_view>

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/borealis/testing/apps.h"
#include "chrome/browser/ash/borealis/testing/windows.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
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

}  // namespace

TEST_F(BorealisUtilTest, GetBorealisAppIdReturnsEmptyOnFailure) {
  EXPECT_EQ(ParseSteamGameId("foo"), std::nullopt);
}

TEST_F(BorealisUtilTest, GetBorealisAppIdReturnsId) {
  EXPECT_EQ(ParseSteamGameId("steam://rungameid/123").value(), 123);
}

TEST_F(BorealisUtilTest, GetBorealisAppIdFromWindowReturnsEmptyOnFailure) {
  std::unique_ptr<aura::Window> window =
      MakeWindow("org.chromium.guest_os.borealis.wmclass.foo");
  EXPECT_EQ(SteamGameId(window.get()), std::nullopt);
}

TEST_F(BorealisUtilTest, GetBorealisAppIdFromWindowReturnsId) {
  std::unique_ptr<aura::Window> window =
      MakeWindow("org.chromium.guest_os.borealis.xprop.123");
  EXPECT_EQ(SteamGameId(window.get()).value(), 123);
}

TEST_F(BorealisUtilTest, IsNonGameBorealisAppReturnsTrueForNonGameBorealisApp) {
  EXPECT_TRUE(IsNonGameBorealisApp(
      "borealis_anon:org.chromium.guest_os.borealis.xid.100"));
}

TEST_F(BorealisUtilTest, IsNonGameBorealisAppReturnsFalseForGames) {
  EXPECT_FALSE(
      IsNonGameBorealisApp("borealis_anon:org.chromium.guest_os.borealis.app"));
}

TEST_F(BorealisUtilTest, SteamGameIdNulloptForUnregistered) {
  TestingProfile prof;
  EXPECT_FALSE(SteamGameId(&prof, "test").has_value());
}

TEST_F(BorealisUtilTest, SteamGameIdNulloptForNonGame) {
  TestingProfile prof;
  CreateFakeMainApp(&prof);
  EXPECT_FALSE(SteamGameId(&prof, kClientAppId).has_value());
}

TEST_F(BorealisUtilTest, SteamGameIdNulloptForAnonNonGame) {
  TestingProfile prof;
  EXPECT_FALSE(
      SteamGameId(&prof,
                  "borealis_anon:org.chromium.guest_os.borealis.xid.1337")
          .has_value());
}

TEST_F(BorealisUtilTest, SteamGameIdWithRegisteredGame) {
  TestingProfile prof;
  CreateFakeApp(&prof, "test", "steam://rungameid/42");
  EXPECT_EQ(SteamGameId(&prof, FakeAppId("test")).value(), 42);
}

TEST_F(BorealisUtilTest, SteamGameIdWithAnonGame) {
  TestingProfile prof;
  EXPECT_EQ(
      SteamGameId(&prof,
                  "borealis_anon:org.chromium.guest_os.borealis.xprop.1337")
          .value(),
      1337);
}

TEST_F(BorealisUtilTest, ProtonTitleUnknownBorealisAppId) {
  std::optional<int> game_id;
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
  std::optional<int> game_id = 123;
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
  std::optional<int> game_id;
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
  std::optional<int> game_id = 123;
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
  std::optional<int> game_id = 123;
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
  std::optional<int> game_id;
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
  std::optional<int> game_id = 123;
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
  std::optional<int> game_id = 123;
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
  std::optional<int> game_id;
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
  std::optional<int> game_id = 123;
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
  std::optional<int> game_id;
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

guest_os::GuestOsRegistryService::Registration CreateRegistration(
    std::string guest_os_app_id,
    std::string_view name,
    std::string_view exec) {
  base::Value pref(base::Value::Type::DICT);
  base::Value::Dict localized_name;
  localized_name.Set("" /* locale */, base::Value(name));
  pref.GetDict().Set(guest_os::prefs::kAppNameKey, std::move(localized_name));
  pref.GetDict().Set(guest_os::prefs::kAppExecKey, exec);
  return guest_os::GuestOsRegistryService::Registration(guest_os_app_id,
                                                        std::move(pref));
}

TEST_F(BorealisUtilTest, HidesFutureProtonTools) {
  EXPECT_TRUE(ShouldHideIrrelevantApp(CreateRegistration(
      "fake app id", "Proton 9.0", "steam://rungameid/999")));
}

TEST_F(BorealisUtilTest, HidesToolsById) {
  EXPECT_TRUE(ShouldHideIrrelevantApp(CreateRegistration(
      "fake app id", "A bold new name for an existing tool",
      "steam://rungameid/1391110"  // Soldier SLR
      )));
}

TEST_F(BorealisUtilTest, DoesNotHideGames) {
  // "Proton Rush" reads like a Proton version, but isn't, so don't hide it.
  // It's also not an actual game (yet?), this is just an example.
  EXPECT_FALSE(ShouldHideIrrelevantApp(CreateRegistration(
      "fake app id", "Proton Rush", "steam://rungameid/123456789")));
}

}  // namespace borealis

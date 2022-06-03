// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_terminal.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace crostini {

using CrostiniTerminalTest = testing::Test;

TEST_F(CrostiniTerminalTest, GenerateTerminalURL) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  EXPECT_EQ(
      GenerateTerminalURL(&profile, "", ContainerId::GetDefault(), "", {}),
      "chrome-untrusted://terminal/html/terminal.html"
      "?command=vmshell"
      "&args[]=--vm_name%3Dtermina"
      "&args[]=--target_container%3Dpenguin"
      "&args[]=--owner_id%3Dtest");
  EXPECT_EQ(GenerateTerminalURL(&profile, "red",
                                ContainerId("test-vm", "test-container"),
                                "/home/user", {"arg1"}),
            "chrome-untrusted://terminal/html/terminal.html"
            "?command=vmshell"
            "&settings_profile=red"
            "&args[]=--vm_name%3Dtest-vm"
            "&args[]=--target_container%3Dtest-container"
            "&args[]=--owner_id%3Dtest"
            "&args[]=--cwd%3D%2Fhome%2Fuser"
            "&args[]=--&args[]=arg1");
}

TEST_F(CrostiniTerminalTest, ShortcutIdForSSH) {
  EXPECT_EQ(ShortcutIdForSSH("test-profile"),
            R"({"profileId":"test-profile","shortcut":"ssh"})");
}

TEST_F(CrostiniTerminalTest, ShortcutIdFromContainerId) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ContainerId id("test-vm", "test-container");
  EXPECT_EQ(ShortcutIdFromContainerId(&profile, id),
            R"({"container_name":"test-container","shortcut":"terminal",)"
            R"("vm_name":"test-vm"})");

  // Container with multi-profile should include settings_profile.
  auto pref = base::JSONReader::Read(R"({
    "/vsh/profile-ids": ["p1", "p2"],
    "/vsh/profiles/p1/vm-name": "test-vm",
    "/vsh/profiles/p1/container-name": "other-container",
    "/vsh/profiles/p1/terminal-profile": "red",
    "/vsh/profiles/p2/vm-name": "test-vm",
    "/vsh/profiles/p2/container-name": "test-container",
    "/vsh/profiles/p2/terminal-profile": "green"
  })");
  ASSERT_TRUE(pref.has_value());
  profile.GetPrefs()->Set(prefs::kCrostiniTerminalSettings, std::move(*pref));
  EXPECT_EQ(ShortcutIdFromContainerId(&profile, id),
            R"({"container_name":"test-container",)"
            R"("settings_profile":"green",)"
            R"("shortcut":"terminal",)"
            R"("vm_name":"test-vm"})");
}

TEST_F(CrostiniTerminalTest, GetSSHConnections) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;

  std::vector<std::pair<std::string, std::string>> expected;
  EXPECT_EQ(GetSSHConnections(&profile), expected);

  auto pref = base::JSONReader::Read(R"({
    "/nassh/profile-ids": ["p1", "p2", "p3"],
    "/nassh/profiles/p1/description": "d1",
    "/nassh/profiles/p2/description": "d2"
  })");
  ASSERT_TRUE(pref.has_value());
  profile.GetPrefs()->Set(prefs::kCrostiniTerminalSettings, std::move(*pref));

  expected = {{"p1", "d1"}, {"p2", "d2"}};
  EXPECT_EQ(GetSSHConnections(&profile), expected);
}

TEST_F(CrostiniTerminalTest, GetTerminalSettingBackgroundColor) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;

  auto pref = base::JSONReader::Read(R"({
    "/hterm/profiles/default/background-color": "#101010",
    "/hterm/profiles/red/background-color": "#FF0000"
  })");
  ASSERT_TRUE(pref.has_value());
  profile.GetPrefs()->Set(prefs::kCrostiniTerminalSettings, std::move(*pref));

  // Use settings_profile param.
  EXPECT_EQ(GetTerminalSettingBackgroundColor(
                &profile,
                GURL("chrome-untrusted://terminal/html/"
                     "terminal.html?settings_profile=red"),
                SK_ColorGREEN),
            "#FF0000");

  // Use opener color.
  EXPECT_EQ(
      GetTerminalSettingBackgroundColor(
          &profile, GURL("chrome-untrusted://terminal/html/terminal.html"),
          SK_ColorGREEN),
      "#00ff00ff");

  // Use color from 'default' profile.
  EXPECT_EQ(
      GetTerminalSettingBackgroundColor(
          &profile, GURL("chrome-untrusted://terminal/html/terminal.html"),
          absl::nullopt),
      "#101010");

  // Use default color.
  profile.GetPrefs()->Set(prefs::kCrostiniTerminalSettings,
                          base::Value(base::Value::Type::DICT));
  EXPECT_EQ(
      GetTerminalSettingBackgroundColor(
          &profile, GURL("chrome-untrusted://terminal/html/terminal.html"),
          absl::nullopt),
      "#202124");
}

}  // namespace crostini

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

namespace crostini {

using CrostiniTerminalTest = testing::Test;

TEST_F(CrostiniTerminalTest, ShortcutIdForSSH) {
  EXPECT_EQ(ShortcutIdForSSH("test-profile"),
            R"({"profileId":"test-profile","shortcut":"ssh"})");
}

TEST_F(CrostiniTerminalTest, ShortcutIdFromContainerId) {
  ContainerId id("test-vm", "test-container");
  EXPECT_EQ(ShortcutIdFromContainerId(id),
            R"({"container_name":"test-container","shortcut":"terminal",)"
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

TEST_F(CrostiniTerminalTest, GetLinuxContainers) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  auto pref = base::JSONReader::Read(R"([
    {"vm_name": "vm1", "container_name": "c1"},
    {"vm_name": "vm2", "container_name": "c2"},
    {"vm_name": "vm3"}
  ])");
  ASSERT_TRUE(pref.has_value());
  profile.GetPrefs()->Set(prefs::kCrostiniContainers, std::move(*pref));
  std::vector<ContainerId> expected = {ContainerId("vm1", "c1"),
                                       ContainerId("vm2", "c2")};
  EXPECT_EQ(GetLinuxContainers(&profile), expected);
}

}  // namespace crostini

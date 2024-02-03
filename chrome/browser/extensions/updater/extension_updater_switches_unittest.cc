// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/extension_updater_switches.h"

#include "base/command_line.h"
#include "chrome/common/channel_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(ExtensionUpdaterSwitchesTest, GetChannelForExtensionUpdates) {
  EXPECT_EQ(chrome::GetChannelName(chrome::WithExtendedStable(true)),
            GetChannelForExtensionUpdates());

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      kSwitchExtensionForceChannel, "channel_for_unittest");

  EXPECT_EQ("channel_for_unittest", GetChannelForExtensionUpdates());
}

}  // namespace extensions

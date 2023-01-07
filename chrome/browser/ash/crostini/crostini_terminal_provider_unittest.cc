// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_terminal_provider.h"

#include "chrome/browser/ash/crostini/crostini_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

class CrostiniTerminalProviderTest : public testing::Test {};

TEST_F(CrostiniTerminalProviderTest, Label) {
  guest_os::GuestId id1(kCrostiniDefaultVmType, "vm_name", "container");
  ASSERT_EQ(CrostiniTerminalProvider(nullptr, id1).Label(),
            "vm_name:container");

  guest_os::GuestId id2(kCrostiniDefaultVmType, "termina", "notpenguin");
  ASSERT_EQ(CrostiniTerminalProvider(nullptr, id2).Label(), "notpenguin");

  // Leave the VM name off the label if it's the default VM.
  ASSERT_EQ(CrostiniTerminalProvider(nullptr, DefaultContainerId()).Label(),
            "penguin");
}

}  // namespace crostini

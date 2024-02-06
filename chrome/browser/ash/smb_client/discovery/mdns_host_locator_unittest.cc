// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/discovery/mdns_host_locator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::smb_client {

class MDnsHostLocatorTest : public testing::Test {
 public:
  MDnsHostLocatorTest() = default;

  MDnsHostLocatorTest(const MDnsHostLocatorTest&) = delete;
  MDnsHostLocatorTest& operator=(const MDnsHostLocatorTest&) = delete;

  ~MDnsHostLocatorTest() override = default;
};

TEST_F(MDnsHostLocatorTest, RemoveLocal) {
  EXPECT_EQ(RemoveLocal("QNAP"), "QNAP");
  EXPECT_EQ(RemoveLocal(".local-QNAP"), ".local-QNAP");
  EXPECT_EQ(RemoveLocal("QNAP.local"), "QNAP");
  EXPECT_EQ(RemoveLocal(".localQNAP.local"), ".localQNAP");
  EXPECT_EQ(RemoveLocal("QNAP.local.local"), "QNAP.local");
  EXPECT_EQ(RemoveLocal("QNAP.LOCAL"), "QNAP");
  EXPECT_EQ(RemoveLocal("QNAP.LoCaL"), "QNAP");
}

}  // namespace ash::smb_client

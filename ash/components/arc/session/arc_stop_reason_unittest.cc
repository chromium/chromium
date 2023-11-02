// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "ash/components/arc/session/arc_stop_reason.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

std::string ConvertToString(ArcStopReason reason) {
  std::stringstream ss;
  ss << reason;
  return ss.str();
}

// Tests "<<" operator for ArcStopReason type.
TEST(ArcStopReasonTest, Default) {
  EXPECT_EQ(ConvertToString(ArcStopReason::SHUTDOWN), "SHUTDOWN");
  EXPECT_EQ(ConvertToString(ArcStopReason::GENERIC_BOOT_FAILURE),
            "GENERIC_BOOT_FAILURE");
  EXPECT_EQ(ConvertToString(ArcStopReason::LOW_DISK_SPACE), "LOW_DISK_SPACE");
  EXPECT_EQ(ConvertToString(ArcStopReason::CRASH), "CRASH");
}

}  // namespace
}  // namespace arc

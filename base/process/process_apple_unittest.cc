// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// Tests that Process::CreationTime() returns a valid time for the current
// process on Apple platforms.
TEST(ProcessAppleTest, CreationTimeCurrentProcess) {
  const Time creation_time = Process::Current().CreationTime();
  EXPECT_FALSE(creation_time.is_null());
  EXPECT_LE(creation_time, Time::Now());
}

}  // namespace base

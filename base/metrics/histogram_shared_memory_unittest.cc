// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_shared_memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(HistogramSharedMemory, Default) {
  HistogramSharedMemory default_constructed;
  EXPECT_FALSE(default_constructed.IsValid());
}

TEST(HistogramSharedMemory, Create) {
  auto memory = HistogramSharedMemory::Create(1234, {"Test", 1 << 20});
  ASSERT_TRUE(memory.has_value());
  EXPECT_TRUE(memory->IsValid());
}

}  // namespace base

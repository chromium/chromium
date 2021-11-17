// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/video_accelerator/arc_video_accelerator_util.h"

#include "base/files/scoped_file.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {
constexpr char kTestData[] = "TEST_DATA";
constexpr size_t kNumFds = 3;
}  // namespace

TEST(ArcVideoAcceleratorUtil, DuplicateFD_OK) {
  base::ScopedFD fd = CreateTempFileForTesting(kTestData);
  auto fds = DuplicateFD(std::move(fd), kNumFds);

  EXPECT_EQ(fds.size(), kNumFds);
}

TEST(ArcVideoAcceleratorUtil, DuplicateFD_Fail) {
  auto fds = DuplicateFD(base::ScopedFD(), kNumFds);

  EXPECT_EQ(fds.size(), 0u);
}

}  // namespace arc

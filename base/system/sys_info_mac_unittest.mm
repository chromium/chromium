// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system/sys_info.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

using SysInfoMacTest = testing::Test;

TEST_F(SysInfoMacTest, SplitHardwareModelName) {
  std::optional<SysInfo::HardwareModelNameSplit> split_name =
      SysInfo::SplitHardwareModelNameDoNotUse("");
  EXPECT_EQ(std::nullopt, split_name);

  split_name = SysInfo::SplitHardwareModelNameDoNotUse("FooBar");
  EXPECT_EQ(std::nullopt, split_name);

  split_name = SysInfo::SplitHardwareModelNameDoNotUse("BarFoo77");
  EXPECT_EQ(std::nullopt, split_name);

  split_name = SysInfo::SplitHardwareModelNameDoNotUse("MacPro4,1");
  EXPECT_EQ("MacPro", split_name.value().category);
  EXPECT_EQ(4, split_name.value().model);
  EXPECT_EQ(1, split_name.value().variant);

  split_name = SysInfo::SplitHardwareModelNameDoNotUse("MacBookPro6,2");
  EXPECT_EQ("MacBookPro", split_name.value().category);
  EXPECT_EQ(6, split_name.value().model);
  EXPECT_EQ(2, split_name.value().variant);
}

}  // namespace

}  // namespace base

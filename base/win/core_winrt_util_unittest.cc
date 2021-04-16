// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/core_winrt_util.h"

#include "base/win/com_init_util.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

TEST(CoreWinrtUtilTest, PreloadFunctions) {
  if (GetVersion() < Version::WIN8)
    EXPECT_FALSE(ResolveCoreWinRTDelayload());
  else
    EXPECT_TRUE(ResolveCoreWinRTDelayload());
}

}  // namespace win
}  // namespace base

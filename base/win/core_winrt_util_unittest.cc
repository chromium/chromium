// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/core_winrt_util.h"

#include "base/win/com_init_util.h"
#include "base/win/scoped_com_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::win {

TEST(CoreWinrtUtilTest, PreloadFunctions) {
  EXPECT_TRUE(ResolveCoreWinRTDelayload());
}

}  // namespace base::win

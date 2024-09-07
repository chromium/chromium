// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/large_icon_service_factory.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

class LargeIconServiceFactoryTest : public testing::Test {};

TEST_F(LargeIconServiceFactoryTest, DesiredSizeInDipForServerRequests) {
#if BUILDFLAG(IS_ANDROID)
  const int expected = 32;
#else
  const int expected = 16;
#endif

  EXPECT_EQ(LargeIconServiceFactory::desired_size_in_dip_for_server_requests(),
            expected);
}

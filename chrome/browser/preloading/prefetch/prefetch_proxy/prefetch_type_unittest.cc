// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_type.h"
#include <array>
#include <tuple>
#include "testing/gtest/include/gtest/gtest.h"

TEST(PrefetchProxyPrefetchTypeTest, WptProxyTest) {
  PrefetchType prefetch_types[] = {
      {/*isolated*/ true, /*use_proxy*/ true, /*subresources*/ true},
      {/*isolated*/ true, /*use_proxy*/ true, /*subresources*/ false},
      {/*isolated*/ true, /*use_proxy*/ false, /*subresources*/ false},
      {/*isolated*/ false, /*use_proxy*/ false, /*subresources*/ false}};
  for (auto& prefetch_type : prefetch_types) {
    EXPECT_FALSE(prefetch_type.IsProxyBypassedForTesting());
    if (prefetch_type.IsProxyRequired()) {
      prefetch_type.SetProxyBypassedForTest();
      EXPECT_TRUE(prefetch_type.IsProxyBypassedForTesting());
    }
  }
}

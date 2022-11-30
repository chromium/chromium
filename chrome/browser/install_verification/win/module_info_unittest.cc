// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/install_verification/win/module_info.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(ModuleInfoTest, TestCase) {
  ModuleInfo foo(L"foo", 0, 10);
  ModuleInfo bar(L"bar", 5, 10);

  ASSERT_LT(foo, bar);

  ASSERT_EQ(L"foo", foo.name);
  ASSERT_TRUE(foo.ContainsAddress(4));
  ASSERT_TRUE(foo.ContainsAddress(5));
  ASSERT_TRUE(foo.ContainsAddress(9));
  ASSERT_FALSE(foo.ContainsAddress(10));
  ASSERT_FALSE(foo.ContainsAddress(11));

  ASSERT_FALSE(bar.ContainsAddress(4));
  ASSERT_TRUE(bar.ContainsAddress(5));
  ASSERT_TRUE(bar.ContainsAddress(9));
  ASSERT_TRUE(bar.ContainsAddress(10));
  ASSERT_TRUE(bar.ContainsAddress(11));
}

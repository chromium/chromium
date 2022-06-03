// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_localalloc.h"

#include <windows.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

TEST(ScopedLocalAlloc, Transfer) {
  HLOCAL ptr = ::LocalAlloc(LMEM_FIXED, 0x1000);
  ASSERT_TRUE(ptr);
  ScopedLocalAllocTyped<void> scoped_ptr = TakeLocalAlloc(ptr);
  EXPECT_TRUE(scoped_ptr);
  EXPECT_FALSE(ptr);
  scoped_ptr.reset();
  EXPECT_FALSE(scoped_ptr);

  wchar_t* str_ptr = static_cast<wchar_t*>(::LocalAlloc(LMEM_FIXED, 0x1000));
  ASSERT_TRUE(str_ptr);
  ScopedLocalAllocTyped<wchar_t> scoped_str_ptr = TakeLocalAlloc(str_ptr);
  EXPECT_TRUE(scoped_str_ptr);
  EXPECT_FALSE(str_ptr);
  scoped_str_ptr.reset();
  EXPECT_FALSE(scoped_str_ptr);
}

}  // namespace win
}  // namespace base

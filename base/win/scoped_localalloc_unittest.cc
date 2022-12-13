// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_localalloc.h"

#include <windows.h>

#include <shellapi.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

TEST(ScopedLocalAlloc, SimpleUsage) {
  ScopedLocalAlloc scoped_local_alloc(::LocalAlloc(LMEM_FIXED, 0x1000));
  EXPECT_TRUE(scoped_local_alloc);
  scoped_local_alloc.reset();
  EXPECT_FALSE(scoped_local_alloc);

  std::wstring input_command_line = L"c:\\test\\process.exe --p1=1";
  int num_args = 0;
  base::win::ScopedLocalAllocTyped<wchar_t*> argv(
      ::CommandLineToArgvW(&input_command_line[0], &num_args));
  EXPECT_TRUE(argv);
  EXPECT_STREQ(argv.get()[0], L"c:\\test\\process.exe");
  argv.reset();
  EXPECT_FALSE(argv);
}

TEST(ScopedLocalAlloc, Transfer) {
  HLOCAL ptr = ::LocalAlloc(LMEM_FIXED, 0x1000);
  ASSERT_TRUE(ptr);
  ScopedLocalAlloc scoped_ptr = TakeLocalAlloc(ptr);
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

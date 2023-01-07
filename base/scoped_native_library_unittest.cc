// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_native_library.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#endif

namespace base {

// Tests whether or not a function pointer retrieved via ScopedNativeLibrary
// is available only in a scope.
TEST(ScopedNativeLibrary, Basic) {
#if BUILDFLAG(IS_WIN)
  // Get the pointer to DirectDrawCreate() from "ddraw.dll" and verify it
  // is valid only in this scope.
  // FreeLibrary() doesn't actually unload a DLL until its reference count
  // becomes zero, i.e. function pointer is still valid if the DLL used
  // in this test is also used by another part of this executable.
  // So, this test uses "ddraw.dll", which is not used by Chrome at all but
  // installed on all versions of Windows.
  const char kFunctionName[] = "DirectDrawCreate";
  NativeLibrary native_library;
  {
    FilePath path(FilePath::FromUTF8Unsafe(GetNativeLibraryName("ddraw")));
    native_library = LoadNativeLibrary(path, nullptr);
    ScopedNativeLibrary library(native_library);
    EXPECT_TRUE(library.is_valid());
    EXPECT_EQ(native_library, library.get());
    FARPROC test_function =
        reinterpret_cast<FARPROC>(library.GetFunctionPointer(kFunctionName));
    EXPECT_EQ(0, IsBadCodePtr(test_function));
    EXPECT_EQ(
        GetFunctionPointerFromNativeLibrary(native_library, kFunctionName),
        test_function);
  }
  EXPECT_FALSE(
      GetFunctionPointerFromNativeLibrary(native_library, kFunctionName));
#endif
}

}  // namespace base

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/elf_reader.h"

#include <dlfcn.h>

#include <string>

#include "base/files/memory_mapped_file.h"
#include "base/native_library.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

extern char __executable_start;

namespace base {
namespace debug {

// The linker flag --build-id is passed only on official builds and Fuchsia
// builds.
#if defined(OFFICIAL_BUILD) || defined(OS_FUCHSIA)

#if defined(OFFICIAL_BUILD)
constexpr size_t kExpectedBuildIdStringLength = 40;  // SHA1 hash in hex.
#else
constexpr size_t kExpectedBuildIdStringLength = 16;  // 64-bit int in hex.
#endif

TEST(ElfReaderTest, ReadElfBuildIdUppercase) {
  ElfBuildIdBuffer build_id;
  size_t build_id_size = ReadElfBuildId(&__executable_start, true, build_id);
  ASSERT_NE(build_id_size, 0u);

  EXPECT_EQ(kExpectedBuildIdStringLength, build_id_size);
  for (size_t i = 0; i < build_id_size; ++i) {
    char c = build_id[i];
    EXPECT_TRUE(IsHexDigit(c));
    EXPECT_FALSE(IsAsciiLower(c));
  }
}

TEST(ElfReaderTest, ReadElfBuildIdLowercase) {
  ElfBuildIdBuffer build_id;
  size_t build_id_size = ReadElfBuildId(&__executable_start, false, build_id);
  ASSERT_NE(build_id_size, 0u);

  EXPECT_EQ(kExpectedBuildIdStringLength, build_id_size);
  for (size_t i = 0; i < kExpectedBuildIdStringLength; ++i) {
    char c = build_id[i];
    EXPECT_TRUE(IsHexDigit(c));
    EXPECT_TRUE(!IsAsciiAlpha(c) || IsAsciiLower(c));
  }
}
#endif  // defined(OFFICIAL_BUILD) || defined(OS_FUCHSIA)

TEST(ElfReaderTest, ReadElfLibraryName) {
#if defined(OS_ANDROID)
  // On Android the library loader memory maps the full so file.
  const char kLibraryName[] = "libbase_unittests__library";
  const void* addr = &__executable_start;
#else
  const char kLibraryName[] = MALLOC_WRAPPER_LIB;
  // On Linux the executable does not contain soname and is not mapped till
  // dynamic segment. So, use malloc wrapper so file on which the test already
  // depends on.
  // Find any symbol in the loaded file.
  //
  NativeLibraryLoadError error;
  NativeLibrary library =
      LoadNativeLibrary(base::FilePath(kLibraryName), &error);
  void* init_addr =
      GetFunctionPointerFromNativeLibrary(library, "MallocWrapper");

  // Use this symbol to get full path to the loaded library.
  Dl_info info;
  int res = dladdr(init_addr, &info);
  ASSERT_NE(0, res);
  const void* addr = info.dli_fbase;
#endif

  auto name = ReadElfLibraryName(addr);
  ASSERT_TRUE(name);
  EXPECT_NE(std::string::npos, name->find(kLibraryName))
      << "Library name " << *name << " doesn't contain expected "
      << kLibraryName;

#if !defined(OS_ANDROID)
  UnloadNativeLibrary(library);
#endif
}

}  // namespace debug
}  // namespace base

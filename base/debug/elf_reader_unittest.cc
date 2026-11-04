// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/elf_reader.h"

#include <dlfcn.h>

#include <cstdint>
#include <optional>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/debug/test_elf_image_builder.h"
#include "base/files/memory_mapped_file.h"
#include "base/native_library.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

extern char __executable_start;

namespace base::debug {

namespace {
constexpr uint8_t kBuildIdBytes[] = {0xab, 0xcd, 0x12, 0x34};
constexpr const char kBuildIdHexString[] = "ABCD1234";
constexpr const char kBuildIdHexStringLower[] = "ABCD1234";

std::string ParamInfoToString(
    const ::testing::TestParamInfo<base::TestElfImageBuilder::MappingType>&
        param_info) {
  switch (param_info.param) {
    case TestElfImageBuilder::RELOCATABLE:
      return "Relocatable";

    case TestElfImageBuilder::RELOCATABLE_WITH_BIAS:
      return "RelocatableWithBias";

    case TestElfImageBuilder::NON_RELOCATABLE:
      return "NonRelocatable";
  }
}
}  // namespace

using ElfReaderTest =
    ::testing::TestWithParam<TestElfImageBuilder::MappingType>;

TEST_P(ElfReaderTest, ReadElfBuildIdUppercase) {
  TestElfImage image =
      TestElfImageBuilder(GetParam())
          .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
          .AddNoteSegment(NT_GNU_BUILD_ID, "GNU", kBuildIdBytes)
          .Build();

  // SAFETY: `image` holds a complete ELF image built in memory.
  EXPECT_EQ(kBuildIdHexString,
            UNSAFE_BUFFERS(ReadElfBuildId(image.elf_start(), true)));
}

TEST_P(ElfReaderTest, ReadElfBuildIdLowercase) {
  TestElfImage image =
      TestElfImageBuilder(GetParam())
          .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
          .AddNoteSegment(NT_GNU_BUILD_ID, "GNU", kBuildIdBytes)
          .Build();

  // SAFETY: `image` holds a complete ELF image built in memory.
  EXPECT_EQ(ToLowerASCII(kBuildIdHexStringLower),
            UNSAFE_BUFFERS(ReadElfBuildId(image.elf_start(), false)));
}

TEST_P(ElfReaderTest, ReadElfBuildIdMultipleNotes) {
  constexpr uint8_t kOtherNoteBytes[] = {0xef, 0x56};

  TestElfImage image =
      TestElfImageBuilder(GetParam())
          .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
          .AddNoteSegment(NT_GNU_BUILD_ID + 1, "ABC", kOtherNoteBytes)
          .AddNoteSegment(NT_GNU_BUILD_ID, "GNU", kBuildIdBytes)
          .Build();

  // SAFETY: `image` holds a complete ELF image built in memory.
  EXPECT_EQ(kBuildIdHexString,
            UNSAFE_BUFFERS(ReadElfBuildId(image.elf_start(), true)));
}

TEST_P(ElfReaderTest, ReadElfBuildIdWrongName) {
  TestElfImage image =
      TestElfImageBuilder(GetParam())
          .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
          .AddNoteSegment(NT_GNU_BUILD_ID, "ABC", kBuildIdBytes)
          .Build();

  // SAFETY: `image` holds a complete ELF image built in memory.
  EXPECT_EQ("", UNSAFE_BUFFERS(ReadElfBuildId(image.elf_start(), true)));
}

TEST_P(ElfReaderTest, ReadElfBuildIdWrongType) {
  TestElfImage image =
      TestElfImageBuilder(GetParam())
          .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
          .AddNoteSegment(NT_GNU_BUILD_ID + 1, "GNU", kBuildIdBytes)
          .Build();

  // SAFETY: `image` holds a complete ELF image built in memory.
  EXPECT_EQ("", UNSAFE_BUFFERS(ReadElfBuildId(image.elf_start(), true)));
}

TEST_P(ElfReaderTest, ReadElfBuildIdNoBuildId) {
  TestElfImage image = TestElfImageBuilder(GetParam())
                           .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
                           .Build();

  // SAFETY: `image` holds a complete ELF image built in memory.
  EXPECT_EQ("", UNSAFE_BUFFERS(ReadElfBuildId(image.elf_start(), true)));
}

TEST_P(ElfReaderTest, ReadElfLibraryName) {
  TestElfImage image = TestElfImageBuilder(GetParam())
                           .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
                           .AddSoName("mysoname")
                           .Build();

  // SAFETY: `image` holds a complete ELF image built in memory.
  std::optional<std::string_view> library_name =
      UNSAFE_BUFFERS(ReadElfLibraryName(image.elf_start()));
  ASSERT_NE(std::nullopt, library_name);
  EXPECT_EQ("mysoname", *library_name);
}

TEST_P(ElfReaderTest, ReadElfLibraryNameNoSoName) {
  TestElfImage image = TestElfImageBuilder(GetParam())
                           .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
                           .Build();

  // SAFETY: `image` holds a complete ELF image built in memory.
  std::optional<std::string_view> library_name =
      UNSAFE_BUFFERS(ReadElfLibraryName(image.elf_start()));
  EXPECT_EQ(std::nullopt, library_name);
}

TEST_P(ElfReaderTest, GetRelocationOffset) {
  TestElfImage image = TestElfImageBuilder(GetParam())
                           .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
                           .Build();

  // SAFETY: `image` holds a complete ELF image built in memory.
  const size_t relocation_offset =
      UNSAFE_BUFFERS(GetRelocationOffset(image.elf_start()));
  switch (GetParam()) {
    case TestElfImageBuilder::RELOCATABLE:
      EXPECT_EQ(reinterpret_cast<size_t>(image.elf_start()), relocation_offset);
      break;

    case TestElfImageBuilder::RELOCATABLE_WITH_BIAS:
      EXPECT_EQ(reinterpret_cast<size_t>(image.elf_start()) -
                    TestElfImageBuilder::kLoadBias,
                relocation_offset);
      break;

    case TestElfImageBuilder::NON_RELOCATABLE:
      EXPECT_EQ(0u, relocation_offset);
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    MappingTypes,
    ElfReaderTest,
    ::testing::Values(TestElfImageBuilder::RELOCATABLE,
                      TestElfImageBuilder::RELOCATABLE_WITH_BIAS,
                      TestElfImageBuilder::NON_RELOCATABLE),
    &ParamInfoToString);

TEST(ElfReaderTestWithCurrentElfImage, ReadElfBuildId) {
  // SAFETY: `__executable_start` is a linker-defined symbol at the start of
  // the ELF image of the module containing this code, which has been loaded,
  // and thus fully mapped into memory.
  std::string build_id =
      UNSAFE_BUFFERS(ReadElfBuildId(&__executable_start, true));
  ASSERT_FALSE(build_id.empty());

#if defined(OFFICIAL_BUILD)
  EXPECT_EQ(40u, build_id.size());  // SHA1 hash in hex.
#else
  // Allow any common BuildId hash, since it depends on linker settings.
  if (build_id.size() != 16 && build_id.size() != 40 && build_id.size() != 64) {
    // LLD's "fast", or sha1, or sha256.
    EXPECT_EQ(16u, build_id.size());
  }
#endif

  for (char c : build_id) {
    EXPECT_TRUE(IsHexDigit(c));
    EXPECT_FALSE(IsAsciiLower(c));
  }
}

TEST(ElfReaderTestWithCurrentImage, ReadElfBuildId) {
#if BUILDFLAG(IS_ANDROID)
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

  // SAFETY: `addr` is the start of a module loaded (and thus fully mapped) by
  // the dynamic loader: either `__executable_start`, a linker-defined symbol at
  // the start of this module, or the `dli_fbase` returned by `dladdr()` for a
  // loaded library.
  auto name = UNSAFE_BUFFERS(ReadElfLibraryName(addr));
  ASSERT_TRUE(name);
  EXPECT_NE(std::string::npos, name->find(kLibraryName))
      << "Library name " << *name << " doesn't contain expected "
      << kLibraryName;

#if !BUILDFLAG(IS_ANDROID)
  UnloadNativeLibrary(library);
#endif
}

}  // namespace base::debug

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/elf_reader.h"

#include <dlfcn.h>

#include <cstdint>

#include "base/debug/test_elf_image_builder.h"
#include "base/files/memory_mapped_file.h"
#include "base/native_library.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

extern char __executable_start;

namespace base {
namespace debug {

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

  ElfBuildIdBuffer build_id;
  size_t build_id_size = ReadElfBuildId(image.elf_start(), true, build_id);
  EXPECT_EQ(8u, build_id_size);
  EXPECT_EQ(kBuildIdHexString, StringPiece(&build_id[0], build_id_size));
}

TEST_P(ElfReaderTest, ReadElfBuildIdLowercase) {
  TestElfImage image =
      TestElfImageBuilder(GetParam())
          .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
          .AddNoteSegment(NT_GNU_BUILD_ID, "GNU", kBuildIdBytes)
          .Build();

  ElfBuildIdBuffer build_id;
  size_t build_id_size = ReadElfBuildId(image.elf_start(), false, build_id);
  EXPECT_EQ(8u, build_id_size);
  EXPECT_EQ(ToLowerASCII(kBuildIdHexStringLower),
            StringPiece(&build_id[0], build_id_size));
}

TEST_P(ElfReaderTest, ReadElfBuildIdMultipleNotes) {
  constexpr uint8_t kOtherNoteBytes[] = {0xef, 0x56};

  TestElfImage image =
      TestElfImageBuilder(GetParam())
          .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
          .AddNoteSegment(NT_GNU_BUILD_ID + 1, "ABC", kOtherNoteBytes)
          .AddNoteSegment(NT_GNU_BUILD_ID, "GNU", kBuildIdBytes)
          .Build();

  ElfBuildIdBuffer build_id;
  size_t build_id_size = ReadElfBuildId(image.elf_start(), true, build_id);
  EXPECT_EQ(8u, build_id_size);
  EXPECT_EQ(kBuildIdHexString, StringPiece(&build_id[0], build_id_size));
}

TEST_P(ElfReaderTest, ReadElfBuildIdWrongName) {
  TestElfImage image =
      TestElfImageBuilder(GetParam())
          .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
          .AddNoteSegment(NT_GNU_BUILD_ID, "ABC", kBuildIdBytes)
          .Build();

  ElfBuildIdBuffer build_id;
  size_t build_id_size = ReadElfBuildId(image.elf_start(), true, build_id);
  EXPECT_EQ(0u, build_id_size);
}

TEST_P(ElfReaderTest, ReadElfBuildIdWrongType) {
  TestElfImage image =
      TestElfImageBuilder(GetParam())
          .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
          .AddNoteSegment(NT_GNU_BUILD_ID + 1, "GNU", kBuildIdBytes)
          .Build();

  ElfBuildIdBuffer build_id;
  size_t build_id_size = ReadElfBuildId(image.elf_start(), true, build_id);
  EXPECT_EQ(0u, build_id_size);
}

TEST_P(ElfReaderTest, ReadElfBuildIdNoBuildId) {
  TestElfImage image = TestElfImageBuilder(GetParam())
                           .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
                           .Build();

  ElfBuildIdBuffer build_id;
  size_t build_id_size = ReadElfBuildId(image.elf_start(), true, build_id);
  EXPECT_EQ(0u, build_id_size);
}

TEST_P(ElfReaderTest, ReadElfLibraryName) {
  TestElfImage image = TestElfImageBuilder(GetParam())
                           .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
                           .AddSoName("mysoname")
                           .Build();

  absl::optional<StringPiece> library_name =
      ReadElfLibraryName(image.elf_start());
  ASSERT_NE(absl::nullopt, library_name);
  EXPECT_EQ("mysoname", *library_name);
}

TEST_P(ElfReaderTest, ReadElfLibraryNameNoSoName) {
  TestElfImage image = TestElfImageBuilder(GetParam())
                           .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
                           .Build();

  absl::optional<StringPiece> library_name =
      ReadElfLibraryName(image.elf_start());
  EXPECT_EQ(absl::nullopt, library_name);
}

TEST_P(ElfReaderTest, GetRelocationOffset) {
  TestElfImage image = TestElfImageBuilder(GetParam())
                           .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
                           .Build();

  switch (GetParam()) {
    case TestElfImageBuilder::RELOCATABLE:
      EXPECT_EQ(reinterpret_cast<size_t>(image.elf_start()),
                GetRelocationOffset(image.elf_start()));
      break;

    case TestElfImageBuilder::RELOCATABLE_WITH_BIAS:
      EXPECT_EQ(reinterpret_cast<size_t>(image.elf_start()) -
                    TestElfImageBuilder::kLoadBias,
                GetRelocationOffset(image.elf_start()));
      break;

    case TestElfImageBuilder::NON_RELOCATABLE:
      EXPECT_EQ(0u, GetRelocationOffset(image.elf_start()));
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
  ElfBuildIdBuffer build_id;
  size_t build_id_size = ReadElfBuildId(&__executable_start, true, build_id);
  ASSERT_NE(build_id_size, 0u);

#if defined(OFFICIAL_BUILD)
  constexpr size_t kExpectedBuildIdStringLength = 40;  // SHA1 hash in hex.
#else
  constexpr size_t kExpectedBuildIdStringLength = 16;  // 64-bit int in hex.
#endif

  EXPECT_EQ(kExpectedBuildIdStringLength, build_id_size);
  for (size_t i = 0; i < build_id_size; ++i) {
    char c = build_id[i];
    EXPECT_TRUE(IsHexDigit(c));
    EXPECT_FALSE(IsAsciiLower(c));
  }
}

TEST(ElfReaderTestWithCurrentImage, ReadElfBuildId) {
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

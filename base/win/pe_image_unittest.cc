// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains unit tests for PEImage.
#include <algorithm>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/win/pe_image.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

namespace {

// Just counts the number of invocations.
bool ImportsCallback(const PEImage& image,
                     LPCSTR module,
                     DWORD ordinal,
                     LPCSTR name,
                     DWORD hint,
                     PIMAGE_THUNK_DATA iat,
                     PVOID cookie) {
  int* count = reinterpret_cast<int*>(cookie);
  (*count)++;
  return true;
}

// Just counts the number of invocations.
bool SectionsCallback(const PEImage& image,
                      PIMAGE_SECTION_HEADER header,
                      PVOID section_start,
                      DWORD section_size,
                      PVOID cookie) {
  int* count = reinterpret_cast<int*>(cookie);
  (*count)++;
  return true;
}

// Just counts the number of invocations.
bool RelocsCallback(const PEImage& image,
                    WORD type,
                    PVOID address,
                    PVOID cookie) {
  int* count = reinterpret_cast<int*>(cookie);
  (*count)++;
  return true;
}

// Just counts the number of invocations.
bool ImportChunksCallback(const PEImage& image,
                          LPCSTR module,
                          PIMAGE_THUNK_DATA name_table,
                          PIMAGE_THUNK_DATA iat,
                          PVOID cookie) {
  int* count = reinterpret_cast<int*>(cookie);
  (*count)++;
  return true;
}

// Just counts the number of invocations.
bool DelayImportChunksCallback(const PEImage& image,
                               PImgDelayDescr delay_descriptor,
                               LPCSTR module,
                               PIMAGE_THUNK_DATA name_table,
                               PIMAGE_THUNK_DATA iat,
                               PVOID cookie) {
  int* count = reinterpret_cast<int*>(cookie);
  (*count)++;
  return true;
}

// Just counts the number of invocations.
bool ExportsCallback(const PEImage& image,
                     DWORD ordinal,
                     DWORD hint,
                     LPCSTR name,
                     PVOID function,
                     LPCSTR forward,
                     PVOID cookie) {
  int* count = reinterpret_cast<int*>(cookie);
  (*count)++;
  return true;
}

base::FilePath GetPEImageTestPath() {
  base::FilePath pe_image_test_path;
  EXPECT_TRUE(PathService::Get(DIR_TEST_DATA, &pe_image_test_path));
  pe_image_test_path = pe_image_test_path.Append(FILE_PATH_LITERAL("pe_image"));
#if defined(ARCH_CPU_ARM64)
  pe_image_test_path =
      pe_image_test_path.Append(FILE_PATH_LITERAL("pe_image_test_arm64.dll"));
#elif defined(ARCH_CPU_X86_64)
  pe_image_test_path =
      pe_image_test_path.Append(FILE_PATH_LITERAL("pe_image_test_64.dll"));
#elif defined(ARCH_CPU_X86)
  pe_image_test_path =
      pe_image_test_path.Append(FILE_PATH_LITERAL("pe_image_test_32.dll"));
#else
#error This platform is not supported.
#endif
  return pe_image_test_path;
}

}  // namespace

// Tests that we are able to enumerate stuff from a PE file, and that
// the actual number of items found matches an expected value.
TEST(PEImageTest, EnumeratesPE) {
  base::FilePath pe_image_test_path = GetPEImageTestPath();

#if defined(ARCH_CPU_ARM64)
  const int kSections = 7;
  const int kImportsDlls = 3;
  const int kDelayDlls = 2;
  const int kExports = 3;
  const int kImports = 72;
  const int kDelayImports = 2;
  const int kRelocs = 740;
#elif defined(ARCH_CPU_64_BITS)
  const int kSections = 6;
  const int kImportsDlls = 2;
  const int kDelayDlls = 2;
  const int kExports = 3;
  const int kImports = 70;
  const int kDelayImports = 2;
  const int kRelocs = 976;
#else
  const int kSections = 5;
  const int kImportsDlls = 2;
  const int kDelayDlls = 2;
  const int kExports = 3;
  const int kImports = 66;
  const int kDelayImports = 2;
  const int kRelocs = 2114;
#endif

  ScopedNativeLibrary module(pe_image_test_path);
  ASSERT_TRUE(module.is_valid());

  PEImage pe(module.get());
  int count = 0;
  EXPECT_TRUE(pe.VerifyMagic());

  pe.EnumSections(SectionsCallback, &count);
  EXPECT_EQ(kSections, count);

  count = 0;
  pe.EnumImportChunks(ImportChunksCallback, &count, nullptr);
  EXPECT_EQ(kImportsDlls, count);

  count = 0;
  pe.EnumDelayImportChunks(DelayImportChunksCallback, &count, nullptr);
  EXPECT_EQ(kDelayDlls, count);

  count = 0;
  pe.EnumExports(ExportsCallback, &count);
  EXPECT_EQ(kExports, count);

  count = 0;
  pe.EnumAllImports(ImportsCallback, &count, nullptr);
  EXPECT_EQ(kImports, count);

  count = 0;
  pe.EnumAllDelayImports(ImportsCallback, &count, nullptr);
  EXPECT_EQ(kDelayImports, count);

  count = 0;
  pe.EnumRelocs(RelocsCallback, &count);
  EXPECT_EQ(kRelocs, count);
}

// Tests that we are able to enumerate stuff from a PE file, and that
// the actual number of items found matches an expected value.
TEST(PEImageTest, EnumeratesPEWithTargetModule) {
  base::FilePath pe_image_test_path = GetPEImageTestPath();
  const char kTargetModuleStatic[] = "user32.dll";
  const char kTargetModuleDelay[] = "cfgmgr32.dll";

  const int kImportsDlls = 1;
  const int kDelayDlls = 1;
  const int kExports = 3;
  const int kImports = 2;
  const int kDelayImports = 1;
#if defined(ARCH_CPU_ARM64)
  const int kSections = 7;
  const int kRelocs = 740;
#elif defined(ARCH_CPU_64_BITS)
  const int kSections = 6;
  const int kRelocs = 976;
#else
  const int kSections = 5;
  const int kRelocs = 2114;
#endif

  ScopedNativeLibrary module(pe_image_test_path);
  ASSERT_TRUE(module.is_valid());

  PEImage pe(module.get());
  int count = 0;
  EXPECT_TRUE(pe.VerifyMagic());

  pe.EnumSections(SectionsCallback, &count);
  EXPECT_EQ(kSections, count);

  count = 0;
  pe.EnumImportChunks(ImportChunksCallback, &count, kTargetModuleStatic);
  EXPECT_EQ(kImportsDlls, count);

  count = 0;
  pe.EnumDelayImportChunks(DelayImportChunksCallback, &count,
                           kTargetModuleDelay);
  EXPECT_EQ(kDelayDlls, count);

  count = 0;
  pe.EnumExports(ExportsCallback, &count);
  EXPECT_EQ(kExports, count);

  count = 0;
  pe.EnumAllImports(ImportsCallback, &count, kTargetModuleStatic);
  EXPECT_EQ(kImports, count);

  count = 0;
  pe.EnumAllDelayImports(ImportsCallback, &count, kTargetModuleDelay);
  EXPECT_EQ(kDelayImports, count);

  count = 0;
  pe.EnumRelocs(RelocsCallback, &count);
  EXPECT_EQ(kRelocs, count);
}

// Tests that we can locate an specific exported symbol, by name and by ordinal.
TEST(PEImageTest, RetrievesExports) {
  ScopedNativeLibrary module(FilePath(FILE_PATH_LITERAL("advapi32.dll")));
  ASSERT_TRUE(module.is_valid());

  PEImage pe(module.get());
  WORD ordinal;

  EXPECT_TRUE(pe.GetProcOrdinal("RegEnumKeyExW", &ordinal));

  FARPROC address1 = pe.GetProcAddress("RegEnumKeyExW");
  FARPROC address2 = pe.GetProcAddress(reinterpret_cast<char*>(ordinal));
  EXPECT_TRUE(address1 != nullptr);
  EXPECT_TRUE(address2 != nullptr);
  EXPECT_TRUE(address1 == address2);
}

// Tests that we can locate a forwarded export.
TEST(PEImageTest, ForwardedExport) {
  base::FilePath pe_image_test_path = GetPEImageTestPath();

  ScopedNativeLibrary module(pe_image_test_path);

  ASSERT_TRUE(module.is_valid());

  PEImage pe(module.get());

  FARPROC addr = pe.GetProcAddress("FwdExport");
  EXPECT_EQ(FARPROC(-1), addr);

  PDWORD export_entry = pe.GetExportEntry("FwdExport");
  EXPECT_NE(nullptr, export_entry);
  PVOID fwd_addr = pe.RVAToAddr(*export_entry);
  const char expected_fwd[] = "KERNEL32.CreateFileA";
  EXPECT_STREQ(expected_fwd, reinterpret_cast<char*>(fwd_addr));
}

// Test that we can get debug id out of a module.
TEST(PEImageTest, GetDebugId) {
  static constexpr char kPdbFileName[] = "advapi32.pdb";
  ScopedNativeLibrary module(FilePath(FILE_PATH_LITERAL("advapi32.dll")));
  ASSERT_TRUE(module.is_valid());

  PEImage pe(module.get());
  GUID guid = {0};
  DWORD age = 0;
  LPCSTR pdb_file = nullptr;
  size_t pdb_file_length = 0;
  EXPECT_TRUE(pe.GetDebugId(&guid, &age, &pdb_file, &pdb_file_length));
  EXPECT_EQ(pdb_file_length, strlen(kPdbFileName));
  EXPECT_STREQ(pdb_file, kPdbFileName);

  // Should be valid to call without parameters.
  EXPECT_TRUE(pe.GetDebugId(nullptr, nullptr, nullptr, nullptr));

  GUID empty_guid = {0};
  EXPECT_TRUE(!IsEqualGUID(empty_guid, guid));
  EXPECT_NE(0U, age);
}

}  // namespace win
}  // namespace base

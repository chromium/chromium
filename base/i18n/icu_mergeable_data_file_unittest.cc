// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icu_mergeable_data_file.h"

#include "base/debug/proc_maps_linux.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/icu_util.h"
#include "base/test/icu_test_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_NACL)
#if ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE

namespace base::i18n {

class IcuMergeableDataFileTest : public testing::Test {
 protected:
  void SetUp() override { ResetGlobalsForTesting(); }
  void TearDown() override {
    ResetGlobalsForTesting();

    // ICU must be set back up in case e.g. a log statement that formats times
    // uses it.
    test::InitializeICUForTesting();
  }
};

TEST_F(IcuMergeableDataFileTest, IcuDataFileMergesCommonPages) {
  // Create two temporary files mocking Ash and Lacros's versions of ICU.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  FilePath ash_path = temp_dir.GetPath().AppendASCII("ash_icudtl.dat");
  FilePath lacros_path = temp_dir.GetPath().AppendASCII("lacros_icudtl.dat");

  uint32_t flags =
      base::File::FLAG_CREATE | base::File::FLAG_READ | base::File::FLAG_WRITE;

  File ash_file(ash_path, flags);
  File lacros_file(lacros_path, flags);
  ASSERT_TRUE(ash_file.IsValid());
  ASSERT_TRUE(lacros_file.IsValid());

  // Prepare some data to use for filling in the mock files.
  std::vector<char> pg0(0x1000, 0x00);
  std::vector<char> pg1(0x1000, 0x11);
  std::vector<char> pg2(0x1000, 0x22);
  std::vector<char> pg3(0x1000, 0x33);
  std::vector<char> pg4(0x0333, 0x44);
  int pg0_sz = pg0.size(), pg1_sz = pg1.size(), pg2_sz = pg2.size(),
      pg3_sz = pg3.size(), pg4_sz = pg4.size();

  // Build Ash's file structure:
  //   0x0000 .. 0x1000  =>  { 0x00, ... }  | Shared
  //   0x1000 .. 0x2000  =>  { 0x11, ... }  | Shared
  //   0x2000 .. 0x3000  =>  { 0x22, ... }
  //   0x3000 .. 0x3333  =>  { 0x44, ... }  | Shared
  ASSERT_EQ(pg0_sz, ash_file.WriteAtCurrentPos(pg0.data(), pg0_sz));
  ASSERT_EQ(pg1_sz, ash_file.WriteAtCurrentPos(pg1.data(), pg1_sz));
  ASSERT_EQ(pg2_sz, ash_file.WriteAtCurrentPos(pg2.data(), pg2_sz));
  ASSERT_EQ(pg4_sz, ash_file.WriteAtCurrentPos(pg4.data(), pg4_sz));
  ASSERT_TRUE(ash_file.Flush());
  ash_file.Close();

  // Build Lacros's file structure:
  //   0x0000 .. 0x1000  =>  { 0x00, ... }  | Shared
  //   0x1000 .. 0x2000  =>  { 0x11, ... }  | Shared
  //   0x2000 .. 0x3000  =>  { 0x33, ... }
  //   0x3000 .. 0x3333  =>  { 0x44, ... }  | Shared
  ASSERT_EQ(pg0_sz, lacros_file.WriteAtCurrentPos(pg0.data(), pg0_sz));
  ASSERT_EQ(pg1_sz, lacros_file.WriteAtCurrentPos(pg1.data(), pg1_sz));
  ASSERT_EQ(pg3_sz, lacros_file.WriteAtCurrentPos(pg3.data(), pg3_sz));
  ASSERT_EQ(pg4_sz, lacros_file.WriteAtCurrentPos(pg4.data(), pg4_sz));
  ASSERT_TRUE(lacros_file.Flush());

  // Load Lacros's file and try to merge against Ash's.
  IcuDataFile icu_data_file;
  ASSERT_TRUE(icu_data_file.Initialize(std::move(lacros_file),
                                       MemoryMappedFile::Region::kWholeFile));
  // NOTE: we need to manually call MergeWithAshVersion with a custom path,
  // because this test will be run in a linux-lacros-rel environment where
  // there's no Ash installed in the default ChromeOS directory.
  ASSERT_TRUE(icu_data_file.MergeWithAshVersion(ash_path));

  // Check that Lacros's file content is correct.
  EXPECT_EQ(0, memcmp(icu_data_file.data() + 0x0000, pg0.data(), pg0_sz));
  EXPECT_EQ(0, memcmp(icu_data_file.data() + 0x1000, pg1.data(), pg1_sz));
  EXPECT_EQ(0, memcmp(icu_data_file.data() + 0x2000, pg3.data(), pg3_sz));
  EXPECT_EQ(0, memcmp(icu_data_file.data() + 0x3000, pg4.data(), pg4_sz));

  // Parse the kernel's memory map structures to check if the merge happened.
  std::string proc_maps;
  std::vector<debug::MappedMemoryRegion> regions;
  ASSERT_TRUE(debug::ReadProcMaps(&proc_maps));
  ASSERT_TRUE(ParseProcMaps(proc_maps, &regions));

  uintptr_t lacros_start = reinterpret_cast<uintptr_t>(icu_data_file.data());
  bool region1_ok = false, region2_ok = false, region3_ok = false;

  for (const auto& region : regions) {
    if (region.start == lacros_start) {
      // 0x0000 .. 0x2000 => Ash (merged)
      EXPECT_EQ(lacros_start + 0x2000, region.end);
      EXPECT_EQ(ash_path.value(), region.path);
      region1_ok = true;
    } else if (region.start == lacros_start + 0x2000) {
      // 0x2000 .. 0x3000 => Lacros (not merged)
      EXPECT_EQ(lacros_start + 0x3000, region.end);
      EXPECT_EQ(lacros_path.value(), region.path);
      region2_ok = true;
    } else if (region.start == lacros_start + 0x3000) {
      // 0x3000 .. 0x3333 => Ash (merged)
      EXPECT_EQ(lacros_start + 0x4000, region.end);  // Page-aligned address.
      EXPECT_EQ(ash_path.value(), region.path);
      region3_ok = true;
    }
  }
  EXPECT_TRUE(region1_ok && region2_ok && region3_ok);
  EXPECT_FALSE(icu_data_file.used_cached_hashes());
}

TEST_F(IcuMergeableDataFileTest, IcuDataFileMergesCommonPagesWithCachedHashes) {
  // Create two temporary files mocking Ash and Lacros's versions of ICU.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  FilePath ash_path = temp_dir.GetPath().AppendASCII("ash_icudtl.dat");
  FilePath lacros_path = temp_dir.GetPath().AppendASCII("lacros_icudtl.dat");

  // Create the hash files as well.
  FilePath ash_hash_path = ash_path.AddExtensionASCII(
      IcuMergeableDataFile::kIcuDataFileHashExtension);
  FilePath lacros_hash_path = lacros_path.AddExtensionASCII(
      IcuMergeableDataFile::kIcuDataFileHashExtension);

  uint32_t flags =
      base::File::FLAG_CREATE | base::File::FLAG_READ | base::File::FLAG_WRITE;

  File ash_file(ash_path, flags);
  File ash_hash_file(ash_hash_path, flags);
  File lacros_file(lacros_path, flags);
  File lacros_hash_file(lacros_hash_path, flags);
  ASSERT_TRUE(ash_file.IsValid());
  ASSERT_TRUE(ash_hash_file.IsValid());
  ASSERT_TRUE(lacros_file.IsValid());
  ASSERT_TRUE(lacros_hash_file.IsValid());

  // Prepare some data to use for filling in the mock files.
  std::vector<char> pg0(0x1000, 0x00);
  std::vector<char> pg1(0x1000, 0x11);
  std::vector<char> pg2(0x1000, 0x22);
  std::vector<char> pg3(0x1000, 0x33);
  std::vector<char> pg4(0x0333, 0x44);
  int pg0_sz = pg0.size(), pg1_sz = pg1.size(), pg2_sz = pg2.size(),
      pg3_sz = pg3.size(), pg4_sz = pg4.size();

  // Build Ash's file structure:
  //   0x0000 .. 0x1000  =>  { 0x00, ... }  | Shared
  //   0x1000 .. 0x2000  =>  { 0x11, ... }  | Shared
  //   0x2000 .. 0x3000  =>  { 0x22, ... }
  //   0x3000 .. 0x3333  =>  { 0x44, ... }  | Shared
  ASSERT_EQ(pg0_sz, ash_file.WriteAtCurrentPos(pg0.data(), pg0_sz));
  ASSERT_EQ(pg1_sz, ash_file.WriteAtCurrentPos(pg1.data(), pg1_sz));
  ASSERT_EQ(pg2_sz, ash_file.WriteAtCurrentPos(pg2.data(), pg2_sz));
  ASSERT_EQ(pg4_sz, ash_file.WriteAtCurrentPos(pg4.data(), pg4_sz));
  ASSERT_TRUE(ash_file.Flush());
  ash_file.Close();
  // Build Ash's hash file structure. Actual hashes don't matter.
  ASSERT_EQ(8, ash_hash_file.WriteAtCurrentPos(
                   "\x00\x00\x00\x00\x00\x00\x00\x00", 8));
  ASSERT_EQ(8, ash_hash_file.WriteAtCurrentPos(
                   "\x11\x11\x11\x11\x11\x11\x11\x11", 8));
  ASSERT_EQ(8, ash_hash_file.WriteAtCurrentPos(
                   "\x22\x22\x22\x22\x22\x22\x22\x22", 8));
  ASSERT_EQ(8, ash_hash_file.WriteAtCurrentPos(
                   "\x44\x44\x44\x44\x44\x44\x44\x44", 8));
  ASSERT_TRUE(ash_hash_file.Flush());
  ash_hash_file.Close();

  // Build Lacros's file structure:
  //   0x0000 .. 0x1000  =>  { 0x00, ... }  | Shared
  //   0x1000 .. 0x2000  =>  { 0x11, ... }  | Shared
  //   0x2000 .. 0x3000  =>  { 0x33, ... }
  //   0x3000 .. 0x3333  =>  { 0x44, ... }  | Shared
  ASSERT_EQ(pg0_sz, lacros_file.WriteAtCurrentPos(pg0.data(), pg0_sz));
  ASSERT_EQ(pg1_sz, lacros_file.WriteAtCurrentPos(pg1.data(), pg1_sz));
  ASSERT_EQ(pg3_sz, lacros_file.WriteAtCurrentPos(pg3.data(), pg3_sz));
  ASSERT_EQ(pg4_sz, lacros_file.WriteAtCurrentPos(pg4.data(), pg4_sz));
  ASSERT_TRUE(lacros_file.Flush());
  // Build Lacros's hash file structure. Actual hashes don't matter.
  ASSERT_EQ(8, lacros_hash_file.WriteAtCurrentPos(
                   "\x00\x00\x00\x00\x00\x00\x00\x00", 8));
  ASSERT_EQ(8, lacros_hash_file.WriteAtCurrentPos(
                   "\x11\x11\x11\x11\x11\x11\x11\x11", 8));
  // NOTE: Simulate hash collision.
  ASSERT_EQ(8, lacros_hash_file.WriteAtCurrentPos(
                   "\x22\x22\x22\x22\x22\x22\x22\x22", 8));
  ASSERT_EQ(8, lacros_hash_file.WriteAtCurrentPos(
                   "\x44\x44\x44\x44\x44\x44\x44\x44", 8));
  ASSERT_TRUE(lacros_hash_file.Flush());
  lacros_hash_file.Close();

  // Load Lacros's file and try to merge against Ash's.
  IcuDataFile icu_data_file;
  ASSERT_TRUE(icu_data_file.Initialize(std::move(lacros_file),
                                       MemoryMappedFile::Region::kWholeFile));
  ASSERT_TRUE(icu_data_file.MergeWithAshVersion(ash_path));

  // Check that Lacros's file content is correct.
  EXPECT_EQ(0, memcmp(icu_data_file.data() + 0x0000, pg0.data(), pg0_sz));
  EXPECT_EQ(0, memcmp(icu_data_file.data() + 0x1000, pg1.data(), pg1_sz));
  EXPECT_EQ(0, memcmp(icu_data_file.data() + 0x2000, pg3.data(), pg3_sz));
  EXPECT_EQ(0, memcmp(icu_data_file.data() + 0x3000, pg4.data(), pg4_sz));

  // Parse the kernel's memory map structures to check if the merge happened.
  std::string proc_maps;
  std::vector<debug::MappedMemoryRegion> regions;
  ASSERT_TRUE(debug::ReadProcMaps(&proc_maps));
  ASSERT_TRUE(ParseProcMaps(proc_maps, &regions));

  uintptr_t lacros_start = reinterpret_cast<uintptr_t>(icu_data_file.data());
  bool region1_ok = false, region2_ok = false, region3_ok = false;

  for (const auto& region : regions) {
    if (region.start == lacros_start) {
      // 0x0000 .. 0x2000 => Ash (merged)
      EXPECT_EQ(lacros_start + 0x2000, region.end);
      EXPECT_EQ(ash_path.value(), region.path);
      region1_ok = true;
    } else if (region.start == lacros_start + 0x2000) {
      // 0x2000 .. 0x3000 => Lacros (not merged)
      EXPECT_EQ(lacros_start + 0x3000, region.end);
      EXPECT_EQ(lacros_path.value(), region.path);
      region2_ok = true;
    } else if (region.start == lacros_start + 0x3000) {
      // 0x3000 .. 0x3333 => Ash (merged)
      EXPECT_EQ(lacros_start + 0x4000, region.end);  // Page-aligned address.
      EXPECT_EQ(ash_path.value(), region.path);
      region3_ok = true;
    }
  }
  EXPECT_TRUE(region1_ok && region2_ok && region3_ok);
  EXPECT_TRUE(icu_data_file.used_cached_hashes());
}

}  // namespace base::i18n

#endif  // ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE
#endif  // !BUILDFLAG(IS_NACL)

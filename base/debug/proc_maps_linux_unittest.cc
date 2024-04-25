// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/debug/proc_maps_linux.h"

#include <stddef.h>
#include <stdint.h>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace debug {

TEST(ProcMapsTest, Empty) {
  std::vector<MappedMemoryRegion> regions;
  EXPECT_TRUE(ParseProcMaps("", &regions));
  EXPECT_EQ(0u, regions.size());
}

TEST(ProcMapsTest, NoSpaces) {
  static const char kNoSpaces[] =
      "00400000-0040b000 r-xp 00002200 fc:00 794418 /bin/cat\n";

  std::vector<MappedMemoryRegion> regions;
  ASSERT_TRUE(ParseProcMaps(kNoSpaces, &regions));
  ASSERT_EQ(1u, regions.size());

  EXPECT_EQ(0x00400000u, regions[0].start);
  EXPECT_EQ(0x0040b000u, regions[0].end);
  EXPECT_EQ(0x00002200u, regions[0].offset);
  EXPECT_EQ("/bin/cat", regions[0].path);
}

TEST(ProcMapsTest, Spaces) {
  static const char kSpaces[] =
      "00400000-0040b000 r-xp 00002200 fc:00 794418 /bin/space cat\n";

  std::vector<MappedMemoryRegion> regions;
  ASSERT_TRUE(ParseProcMaps(kSpaces, &regions));
  ASSERT_EQ(1u, regions.size());

  EXPECT_EQ(0x00400000u, regions[0].start);
  EXPECT_EQ(0x0040b000u, regions[0].end);
  EXPECT_EQ(0x00002200u, regions[0].offset);
  EXPECT_EQ("/bin/space cat", regions[0].path);
}

TEST(ProcMapsTest, NoNewline) {
  static const char kNoSpaces[] =
      "00400000-0040b000 r-xp 00002200 fc:00 794418 /bin/cat";

  std::vector<MappedMemoryRegion> regions;
  ASSERT_FALSE(ParseProcMaps(kNoSpaces, &regions));
}

TEST(ProcMapsTest, NoPath) {
  static const char kNoPath[] =
      "00400000-0040b000 rw-p 00000000 00:00 0 \n";

  std::vector<MappedMemoryRegion> regions;
  ASSERT_TRUE(ParseProcMaps(kNoPath, &regions));
  ASSERT_EQ(1u, regions.size());

  EXPECT_EQ(0x00400000u, regions[0].start);
  EXPECT_EQ(0x0040b000u, regions[0].end);
  EXPECT_EQ(0x00000000u, regions[0].offset);
  EXPECT_EQ("", regions[0].path);
}

TEST(ProcMapsTest, Heap) {
  static const char kHeap[] =
      "022ac000-022cd000 rw-p 00000000 00:00 0 [heap]\n";

  std::vector<MappedMemoryRegion> regions;
  ASSERT_TRUE(ParseProcMaps(kHeap, &regions));
  ASSERT_EQ(1u, regions.size());

  EXPECT_EQ(0x022ac000u, regions[0].start);
  EXPECT_EQ(0x022cd000u, regions[0].end);
  EXPECT_EQ(0x00000000u, regions[0].offset);
  EXPECT_EQ("[heap]", regions[0].path);
}

#if defined(ARCH_CPU_32_BITS)
TEST(ProcMapsTest, Stack32) {
  static const char kStack[] =
      "beb04000-beb25000 rw-p 00000000 00:00 0 [stack]\n";

  std::vector<MappedMemoryRegion> regions;
  ASSERT_TRUE(ParseProcMaps(kStack, &regions));
  ASSERT_EQ(1u, regions.size());

  EXPECT_EQ(0xbeb04000u, regions[0].start);
  EXPECT_EQ(0xbeb25000u, regions[0].end);
  EXPECT_EQ(0x00000000u, regions[0].offset);
  EXPECT_EQ("[stack]", regions[0].path);
}
#elif defined(ARCH_CPU_64_BITS)
TEST(ProcMapsTest, Stack64) {
  static const char kStack[] =
      "7fff69c5b000-7fff69c7d000 rw-p 00000000 00:00 0 [stack]\n";

  std::vector<MappedMemoryRegion> regions;
  ASSERT_TRUE(ParseProcMaps(kStack, &regions));
  ASSERT_EQ(1u, regions.size());

  EXPECT_EQ(0x7fff69c5b000u, regions[0].start);
  EXPECT_EQ(0x7fff69c7d000u, regions[0].end);
  EXPECT_EQ(0x00000000u, regions[0].offset);
  EXPECT_EQ("[stack]", regions[0].path);
}
#endif

TEST(ProcMapsTest, Multiple) {
  static const char kMultiple[] =
      "00400000-0040b000 r-xp 00000000 fc:00 794418 /bin/cat\n"
      "0060a000-0060b000 r--p 0000a000 fc:00 794418 /bin/cat\n"
      "0060b000-0060c000 rw-p 0000b000 fc:00 794418 /bin/cat\n";

  std::vector<MappedMemoryRegion> regions;
  ASSERT_TRUE(ParseProcMaps(kMultiple, &regions));
  ASSERT_EQ(3u, regions.size());

  EXPECT_EQ(0x00400000u, regions[0].start);
  EXPECT_EQ(0x0040b000u, regions[0].end);
  EXPECT_EQ(0x00000000u, regions[0].offset);
  EXPECT_EQ("/bin/cat", regions[0].path);

  EXPECT_EQ(0x0060a000u, regions[1].start);
  EXPECT_EQ(0x0060b000u, regions[1].end);
  EXPECT_EQ(0x0000a000u, regions[1].offset);
  EXPECT_EQ("/bin/cat", regions[1].path);

  EXPECT_EQ(0x0060b000u, regions[2].start);
  EXPECT_EQ(0x0060c000u, regions[2].end);
  EXPECT_EQ(0x0000b000u, regions[2].offset);
  EXPECT_EQ("/bin/cat", regions[2].path);
}

TEST(ProcMapsTest, Permissions) {
  static struct {
    const char* input;
    uint8_t permissions;
  } kTestCases[] = {
    {"00400000-0040b000 ---s 00000000 fc:00 794418 /bin/cat\n", 0},
    {"00400000-0040b000 ---S 00000000 fc:00 794418 /bin/cat\n", 0},
    {"00400000-0040b000 r--s 00000000 fc:00 794418 /bin/cat\n",
     MappedMemoryRegion::READ},
    {"00400000-0040b000 -w-s 00000000 fc:00 794418 /bin/cat\n",
     MappedMemoryRegion::WRITE},
    {"00400000-0040b000 --xs 00000000 fc:00 794418 /bin/cat\n",
     MappedMemoryRegion::EXECUTE},
    {"00400000-0040b000 rwxs 00000000 fc:00 794418 /bin/cat\n",
     MappedMemoryRegion::READ | MappedMemoryRegion::WRITE |
         MappedMemoryRegion::EXECUTE},
    {"00400000-0040b000 ---p 00000000 fc:00 794418 /bin/cat\n",
     MappedMemoryRegion::PRIVATE},
    {"00400000-0040b000 r--p 00000000 fc:00 794418 /bin/cat\n",
     MappedMemoryRegion::READ | MappedMemoryRegion::PRIVATE},
    {"00400000-0040b000 -w-p 00000000 fc:00 794418 /bin/cat\n",
     MappedMemoryRegion::WRITE | MappedMemoryRegion::PRIVATE},
    {"00400000-0040b000 --xp 00000000 fc:00 794418 /bin/cat\n",
     MappedMemoryRegion::EXECUTE | MappedMemoryRegion::PRIVATE},
    {"00400000-0040b000 rwxp 00000000 fc:00 794418 /bin/cat\n",
     MappedMemoryRegion::READ | MappedMemoryRegion::WRITE |
         MappedMemoryRegion::EXECUTE | MappedMemoryRegion::PRIVATE},
  };

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    SCOPED_TRACE(
        base::StringPrintf("kTestCases[%zu] = %s", i, kTestCases[i].input));

    std::vector<MappedMemoryRegion> regions;
    EXPECT_TRUE(ParseProcMaps(kTestCases[i].input, &regions));
    EXPECT_EQ(1u, regions.size());
    if (regions.empty())
      continue;
    EXPECT_EQ(kTestCases[i].permissions, regions[0].permissions);
  }
}

// AddressSanitizer may move local variables to a dedicated "fake stack" which
// is outside the stack region listed in /proc/self/maps. We disable ASan
// instrumentation for this function to force the variable to be local.
//
// Similarly, HWAddressSanitizer may add a tag to all stack pointers which may
// move it outside of the stack regions in /proc/self/maps.
__attribute__((no_sanitize("address", "hwaddress"))) void CheckProcMapsRegions(
    const std::vector<MappedMemoryRegion>& regions) {
  // We should be able to find both the current executable as well as the stack
  // mapped into memory. Use the address of |exe_path| as a way of finding the
  // stack.
  FilePath exe_path;
  EXPECT_TRUE(PathService::Get(FILE_EXE, &exe_path));
  uintptr_t address = reinterpret_cast<uintptr_t>(&exe_path);
  bool found_exe = false;
  bool found_stack = false;
  bool found_address = false;

  for (const auto& i : regions) {
    if (i.path == exe_path.value()) {
      // It's OK to find the executable mapped multiple times as there'll be
      // multiple sections (e.g., text, data).
      found_exe = true;
    }

    if (i.path == "[stack]") {
// On Android the test is run on a background thread, since [stack] is for
// the main thread, we cannot test this.
#if !BUILDFLAG(IS_ANDROID)
      EXPECT_GE(address, i.start);
      EXPECT_LT(address, i.end);
#endif
      EXPECT_TRUE(i.permissions & MappedMemoryRegion::READ);
      EXPECT_TRUE(i.permissions & MappedMemoryRegion::WRITE);
      EXPECT_FALSE(i.permissions & MappedMemoryRegion::EXECUTE);
      EXPECT_TRUE(i.permissions & MappedMemoryRegion::PRIVATE);
      EXPECT_FALSE(found_stack) << "Found duplicate stacks";
      found_stack = true;
    }

    if (address >= i.start && address < i.end) {
      EXPECT_FALSE(found_address) << "Found same address in multiple regions";
      found_address = true;
    }
  }

  EXPECT_TRUE(found_exe);
  EXPECT_TRUE(found_stack);
  EXPECT_TRUE(found_address);
}

TEST(ProcMapsTest, ReadProcMaps) {
  std::string proc_maps;
  ASSERT_TRUE(ReadProcMaps(&proc_maps));

  std::vector<MappedMemoryRegion> regions;
  ASSERT_TRUE(ParseProcMaps(proc_maps, &regions));
  ASSERT_FALSE(regions.empty());

  CheckProcMapsRegions(regions);
}

TEST(ProcMapsTest, ReadProcMapsNonEmptyString) {
  std::string old_string("I forgot to clear the string");
  std::string proc_maps(old_string);
  ASSERT_TRUE(ReadProcMaps(&proc_maps));
  EXPECT_EQ(std::string::npos, proc_maps.find(old_string));
}

TEST(ProcMapsTest, MissingFields) {
  static const char* const kTestCases[] = {
    "00400000\n",                               // Missing end + beyond.
    "00400000-0040b000\n",                      // Missing perms + beyond.
    "00400000-0040b000 r-xp\n",                 // Missing offset + beyond.
    "00400000-0040b000 r-xp 00000000\n",        // Missing device + beyond.
    "00400000-0040b000 r-xp 00000000 fc:00\n",  // Missing inode + beyond.
    "00400000-0040b000 00000000 fc:00 794418 /bin/cat\n",  // Missing perms.
    "00400000-0040b000 r-xp fc:00 794418 /bin/cat\n",      // Missing offset.
    "00400000-0040b000 r-xp 00000000 fc:00 /bin/cat\n",    // Missing inode.
    "00400000 r-xp 00000000 fc:00 794418 /bin/cat\n",      // Missing end.
    "-0040b000 r-xp 00000000 fc:00 794418 /bin/cat\n",     // Missing start.
    "00400000-0040b000 r-xp 00000000 794418 /bin/cat\n",   // Missing device.
  };

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestCases[%zu] = %s", i, kTestCases[i]));
    std::vector<MappedMemoryRegion> regions;
    EXPECT_FALSE(ParseProcMaps(kTestCases[i], &regions));
  }
}

TEST(ProcMapsTest, InvalidInput) {
  static const char* const kTestCases[] = {
    "thisisal-0040b000 rwxp 00000000 fc:00 794418 /bin/cat\n",
    "0040000d-linvalid rwxp 00000000 fc:00 794418 /bin/cat\n",
    "00400000-0040b000 inpu 00000000 fc:00 794418 /bin/cat\n",
    "00400000-0040b000 rwxp tforproc fc:00 794418 /bin/cat\n",
    "00400000-0040b000 rwxp 00000000 ma:ps 794418 /bin/cat\n",
    "00400000-0040b000 rwxp 00000000 fc:00 parse! /bin/cat\n",
  };

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestCases[%zu] = %s", i, kTestCases[i]));
    std::vector<MappedMemoryRegion> regions;
    EXPECT_FALSE(ParseProcMaps(kTestCases[i], &regions));
  }
}

TEST(ProcMapsTest, ParseProcMapsEmptyString) {
  std::vector<MappedMemoryRegion> regions;
  EXPECT_TRUE(ParseProcMaps("", &regions));
  EXPECT_EQ(0ULL, regions.size());
}

// Testing a couple of remotely possible weird things in the input:
// - Line ending with \r\n or \n\r.
// - File name contains quotes.
// - File name has whitespaces.
TEST(ProcMapsTest, ParseProcMapsWeirdCorrectInput) {
  std::vector<MappedMemoryRegion> regions;
  const std::string kContents =
    "00400000-0040b000 r-xp 00000000 fc:00 2106562 "
      "               /bin/cat\r\n"
    "7f53b7dad000-7f53b7f62000 r-xp 00000000 fc:00 263011 "
      "       /lib/x86_64-linux-gnu/libc-2.15.so\n\r"
    "7f53b816d000-7f53b818f000 r-xp 00000000 fc:00 264284 "
      "        /lib/x86_64-linux-gnu/ld-2.15.so\n"
    "7fff9c7ff000-7fff9c800000 r-xp 00000000 00:00 0 "
      "               \"vd so\"\n"
    "ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0 "
      "               [vsys call]\n";
  EXPECT_TRUE(ParseProcMaps(kContents, &regions));
  EXPECT_EQ(5ULL, regions.size());
  EXPECT_EQ("/bin/cat", regions[0].path);
  EXPECT_EQ("/lib/x86_64-linux-gnu/libc-2.15.so", regions[1].path);
  EXPECT_EQ("/lib/x86_64-linux-gnu/ld-2.15.so", regions[2].path);
  EXPECT_EQ("\"vd so\"", regions[3].path);
  EXPECT_EQ("[vsys call]", regions[4].path);
}

}  // namespace debug
}  // namespace base

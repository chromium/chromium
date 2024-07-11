// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include <unistd.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/process/set_process_title_linux.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/profiler/module_cache.h"
#endif

namespace {

const std::string kNullChr(1, '\0');

std::string ReadCmdline() {
  std::string cmdline;
  CHECK(base::ReadFileToString(base::FilePath("/proc/self/cmdline"), &cmdline));
  // The process title appears in different formats depending on Linux kernel
  // version:
  // "title"            (on Linux --4.17)
  // "title\0\0\0...\0" (on Linux 4.18--5.2)
  // "title\0"          (on Linux 5.3--)
  //
  // For unit tests, just trim trailing null characters to support all cases.
  return std::string(base::TrimString(cmdline, kNullChr, base::TRIM_TRAILING));
}

TEST(SetProcTitleLinuxTest, Simple) {
  setproctitle("a %s cat", "cute");
  EXPECT_TRUE(base::EndsWith(ReadCmdline(), " a cute cat",
                             base::CompareCase::SENSITIVE))
      << ReadCmdline();

  setproctitle("-a %s cat", "cute");
  EXPECT_EQ(ReadCmdline(), "a cute cat");
}

TEST(SetProcTitleLinuxTest, Empty) {
  setproctitle("-");
  EXPECT_EQ(ReadCmdline(), "");
}

TEST(SetProcTitleLinuxTest, Long) {
  setproctitle("-long cat is l%0100000dng", 0);
  EXPECT_TRUE(base::StartsWith(ReadCmdline(), "long cat is l00000000",
                               base::CompareCase::SENSITIVE))
      << ReadCmdline();
}

#if BUILDFLAG(IS_CHROMEOS)
TEST(SetProcTitleLinuxTest, GetModuleForAddressWorksWithSetProcTitle) {
  // Ensure that after calling setproctitle(), GetModuleForAddress() returns a
  // Module with a valid GetDebugBasename(), not something that includes all the
  // command-line flags. The code we're testing is actually in
  // base/profiler/module_cache_posix.cc, but we need to test it here for
  // dependencies.
  setproctitle("%s", "/opt/google/chrome/chrome --type=renderer --foo=bar");

  base::ModuleCache module_cache;
  // We're assuming the code in this file is linked into the main unittest
  // binary not a shared library.
  const base::ModuleCache::Module* module = module_cache.GetModuleForAddress(
      reinterpret_cast<uintptr_t>(&ReadCmdline));
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->GetDebugBasename().value(), "chrome");
}

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

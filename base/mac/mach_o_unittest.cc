// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/mach_o.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

TEST(MachO, GetMachOArchitectures) {
  FilePath exe_path;
  ASSERT_TRUE(PathService::Get(FILE_EXE, &exe_path));
#if defined(ARCH_CPU_X86_64)
  constexpr MachOArchitectures kSelfArchitecture = MachOArchitectures::kX86_64;
#elif defined(ARCH_CPU_ARM64)
  constexpr MachOArchitectures kSelfArchitecture = MachOArchitectures::kARM64;
#endif
  EXPECT_EQ(GetMachOArchitectures(exe_path) & kSelfArchitecture,
            kSelfArchitecture);

  FilePath data_dir;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &data_dir));
  data_dir = data_dir.AppendASCII("mac");

  EXPECT_EQ(GetMachOArchitectures(data_dir.AppendASCII("x86_64")),
            MachOArchitectures::kX86_64);

  EXPECT_EQ(GetMachOArchitectures(data_dir.AppendASCII("arm64")),
            MachOArchitectures::kARM64);

  EXPECT_EQ(GetMachOArchitectures(data_dir.AppendASCII("universal")),
            MachOArchitectures::kX86_64 | MachOArchitectures::kARM64);

  EXPECT_EQ(GetMachOArchitectures(data_dir.AppendASCII("x86")),
            MachOArchitectures::kUnknownArchitecture);

  EXPECT_EQ(GetMachOArchitectures(data_dir.AppendASCII("elf")),
            MachOArchitectures::kInvalidFormat);

  EXPECT_EQ(GetMachOArchitectures(data_dir.AppendASCII("enoent")),
            MachOArchitectures::kFileError);
}

}  // namespace
}  // namespace base

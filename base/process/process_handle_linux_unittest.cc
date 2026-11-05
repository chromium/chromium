// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_handle.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/base_paths.h"
#include "base/path_service.h"
#endif

namespace base {
namespace {

TEST(ProcessHandleLinuxTest, GetProcessExecutablePathSubstituteProcDir) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const FilePath& proc_dir = temp_dir.GetPath();

  FilePath pid_dir = proc_dir.Append("100");
  ASSERT_TRUE(CreateDirectory(pid_dir));
  const FilePath expected_exe("/usr/bin/my_app");
  ASSERT_TRUE(CreateSymbolicLink(expected_exe, pid_dir.Append("exe")));
  EXPECT_EQ(GetProcessExecutablePath(100, proc_dir), expected_exe);

  // Missing exe symlink returns an empty path.
  FilePath no_exe_dir = proc_dir.Append("200");
  ASSERT_TRUE(CreateDirectory(no_exe_dir));
  EXPECT_TRUE(GetProcessExecutablePath(200, proc_dir).empty());

  // Nonexistent PID returns an empty path.
  EXPECT_TRUE(GetProcessExecutablePath(300, proc_dir).empty());
}

TEST(ProcessHandleLinuxTest, GetProcessExecutablePathSelf) {
  const FilePath path = GetProcessExecutablePath(GetCurrentProcId());
#if BUILDFLAG(IS_ANDROID)
  // On Android, unit tests run inside an APK process forked from Zygote
  // (e.g. /system/bin/app_process64) rather than a standalone `base_unittests`
  // binary, and some OEM SELinux policies block reading /proc/self/exe
  // (https://crbug.com/1416753). Compare against `FILE_EXE` when available.
  FilePath expected;
  if (PathService::Get(FILE_EXE, &expected)) {
    EXPECT_EQ(path, expected);
  }
#else
  EXPECT_EQ(path.BaseName().value(), "base_unittests");
#endif
}

}  // namespace
}  // namespace base

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/launch.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/posix/safe_strerror.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

void LaunchMacTest(const FilePath& exe_path,
                   LaunchOptions options,
                   const std::string& expected_output) {
  ScopedFD pipe_read_fd;
  ScopedFD pipe_write_fd;
  {
    int pipe_fds[2];
    ASSERT_EQ(pipe(pipe_fds), 0) << safe_strerror(errno);
    pipe_read_fd.reset(pipe_fds[0]);
    pipe_write_fd.reset(pipe_fds[1]);
  }

  ScopedFILE pipe_read_file(fdopen(pipe_read_fd.get(), "r"));
  ASSERT_TRUE(pipe_read_file) << "fdopen: " << safe_strerror(errno);
  ignore_result(pipe_read_fd.release());

  std::vector<std::string> argv(1, exe_path.value());
  options.fds_to_remap.emplace_back(pipe_write_fd.get(), STDOUT_FILENO);
  Process process = LaunchProcess(argv, options);
  ASSERT_TRUE(process.IsValid());
  pipe_write_fd.reset();

  // Not ASSERT_TRUE because it's important to reach process.WaitForExit.
  std::string output;
  EXPECT_TRUE(ReadStreamToString(pipe_read_file.get(), &output));

  int exit_code;
  ASSERT_TRUE(process.WaitForExit(&exit_code));
  EXPECT_EQ(exit_code, 0);

  EXPECT_EQ(output, expected_output);
}

#if defined(ARCH_CPU_ARM64)
// Bulk-disabled on arm64 for bot stabilization: https://crbug.com/1154345
#define MAYBE_LaunchMac DISABLED_LaunchMac
#else
#define MAYBE_LaunchMac LaunchMac
#endif
TEST(Process, MAYBE_LaunchMac) {
  FilePath data_dir;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &data_dir));
  data_dir = data_dir.AppendASCII("mac");

#if defined(ARCH_CPU_X86_64)
  static constexpr char kArchitecture[] = "x86_64";
#elif defined(ARCH_CPU_ARM64)
  static constexpr char kArchitecture[] = "arm64";
#endif

  LaunchOptions options;
  LaunchMacTest(data_dir.AppendASCII(kArchitecture), options,
                std::string(kArchitecture) + "\n");

#if defined(ARCH_CPU_ARM64)
  static constexpr char kUniversal[] = "universal";

  LaunchMacTest(data_dir.AppendASCII(kUniversal), options,
                std::string(kArchitecture) + "\n");

  static constexpr char kX86_64[] = "x86_64";

  LaunchMacTest(data_dir.AppendASCII(kX86_64), options,
                std::string(kX86_64) + "\n");

  options.launch_x86_64 = true;
  LaunchMacTest(data_dir.AppendASCII(kUniversal), options,
                std::string(kX86_64) + "\n");

  LaunchMacTest(data_dir.AppendASCII(kX86_64), options,
                std::string(kX86_64) + "\n");
#endif  // ARCH_CPU_ARM64
}

}  // namespace
}  // namespace base

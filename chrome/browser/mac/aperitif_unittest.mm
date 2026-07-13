// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <unistd.h>

#include <set>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"

TEST(AperitifTest, InitializerOrder) {
  base::FilePath dir_exe_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &dir_exe_path));
  base::FilePath helper_path = dir_exe_path.Append("aperitif_test_helper");

  // Run "aperitif_test_helper" with the
  // DYLD_PRINT_INITIALIZERS environment variable set. This will print every
  // module constructor that runs, collecting the results into a buffer.
  base::ScopedFD read_fd, write_fd;
  ASSERT_TRUE(base::CreatePipe(&read_fd, &write_fd));

  base::ScopedFD null_fd(HANDLE_EINTR(open("/dev/null", O_WRONLY)));
  ASSERT_TRUE(null_fd.is_valid());

  base::LaunchOptions options;
  options.environment["DYLD_PRINT_INITIALIZERS"] = "1";
  options.fds_to_remap.emplace_back(null_fd.get(), STDOUT_FILENO);
  options.fds_to_remap.emplace_back(write_fd.get(), STDERR_FILENO);

  base::CommandLine command_line(helper_path);

  base::Process process = base::LaunchProcess(command_line, options);

  // Close the write end in the parent so that the read loop will eventually
  // receive EOF when the child process exits and closes its file descriptors.
  write_fd.reset();

  // Read the stderr output from DYLD_PRINT_INITIALIZERS FIRST to avoid
  // deadlock.
  std::string stderr_output;
  const size_t kBufferSize = 1024;
  size_t total_bytes_read = 0;
  ssize_t read_this_pass = 0;
  do {
    stderr_output.resize(total_bytes_read + kBufferSize);
    read_this_pass = HANDLE_EINTR(
        read(read_fd.get(), &stderr_output[total_bytes_read], kBufferSize));
    if (read_this_pass > 0) {
      total_bytes_read += read_this_pass;
    }
  } while (read_this_pass > 0);
  stderr_output.resize(total_bytes_read);

  // Now wait for the process to exit cleanly.
  int rv = -1;
  ASSERT_TRUE(process.WaitForExit(&rv));
  EXPECT_EQ(0, rv);

  // Split the messages into a vector.
  std::vector<std::string> messages = base::SplitString(
      stderr_output, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  const std::string pattern(
      R"(dyld\[\d+\]: running initializer 0x[0-9a-f]+ in (.*)$)");

  // The set of allowed libraries. Component builds have the dependent
  // libraries of Aperitif.
  // **Do not add more libraries to this set without talking to
  // security-dev@chromium.org!**
  std::set<std::string> allowed_libraries({"/usr/lib/libSystem.B.dylib"});
#if defined(COMPONENT_BUILD)
  allowed_libraries.insert(dir_exe_path.Append("libc++_chrome.dylib").value());
  allowed_libraries.insert(
      dir_exe_path.Append("libprotobuf_lite.dylib").value());
#endif
  const std::string kLibaperitifName = "libaperitif.dylib";

  // Check that only libSystem or libaperitif ran initializers before other
  // libraries.
  bool found_first_disallowed_library = false;
  bool found_libaperitif = false;
  for (size_t i = 0; i < messages.size(); ++i) {
    std::string initializer_path;
    EXPECT_TRUE(re2::RE2::FullMatch(messages[i], pattern, &initializer_path));

    bool is_libaperitif =
        initializer_path.find(kLibaperitifName) != std::string::npos;

#if defined(COMPONENT_BUILD)
    bool is_component_dylib = base::StartsWith(
        initializer_path, dir_exe_path.value(), base::CompareCase::SENSITIVE);
#else
    bool is_component_dylib = false;
#endif

    bool is_allowed = is_libaperitif || is_component_dylib ||
                      allowed_libraries.count(initializer_path) ||
                      base::StartsWith(initializer_path, "/usr/lib/",
                                       base::CompareCase::SENSITIVE) ||
                      base::StartsWith(initializer_path, "/System/Library/",
                                       base::CompareCase::SENSITIVE);

    if (is_allowed) {
      found_libaperitif |= is_libaperitif;
      ASSERT_FALSE(found_first_disallowed_library)
          << "This initializer ran before " << kLibaperitifName
          << " finished initializing: " << messages[i - 1];
    } else {
      found_first_disallowed_library = true;
    }
  }

  EXPECT_TRUE(found_libaperitif);
}

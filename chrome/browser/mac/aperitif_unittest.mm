// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <unistd.h>

#include <set>
#include <string>
#include <vector>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/content_switches.h"
#include "sandbox/mac/sandbox_serializer.h"
#include "sandbox/mac/seatbelt_exec.h"
#include "sandbox/policy/mac/sandbox_mac.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"

TEST(AperitifTest, InitializerOrder) {
  // On macOS 13 Ventura, setting DYLD_PRINT_INITIALIZERS=1 causes Apple's
  // dynamic linker to crash with EXC_BAD_ACCESS (KERN_PROTECTION_FAILURE)
  // inside dyld4::RuntimeState::setUpLogging() due to writing to a sealed dyld
  // private memory page. Apple fixed this in macOS 14 Sonoma. On macOS 13, the
  // helper is launched without DYLD_PRINT_INITIALIZERS to verify that it loads
  // and executes without crashing.
  const bool check_initializer_order = base::mac::MacOSMajorVersion() >= 14;

  base::FilePath dir_exe_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &dir_exe_path));

  base::FilePath framework_path = dir_exe_path.Append(chrome::kFrameworkName)
                                      .Append("Versions")
                                      .Append("Current");
  base::apple::SetOverrideFrameworkBundlePath(framework_path);

  std::string helper_name =
      base::StrCat({chrome::kHelperProcessExecutableName, " (Aperitif)"});
  base::FilePath helper_path =
      dir_exe_path
          .Append(base::StrCat({chrome::kBrowserProcessExecutableName, ".app"}))
          .Append("Contents")
          .Append("Frameworks")
          .Append(chrome::kFrameworkName)
          .Append("Versions")
          .Append("Current")
          .Append("Helpers")
          .Append(helper_name + ".app")
          .Append("Contents")
          .Append("MacOS")
          .Append(helper_name);

  base::ScopedFD read_fd, write_fd;
  ASSERT_TRUE(base::CreatePipe(&read_fd, &write_fd));

  base::ScopedFD null_fd(HANDLE_EINTR(open("/dev/null", O_WRONLY)));
  ASSERT_TRUE(null_fd.is_valid());

  sandbox::SeatbeltExecClient seatbelt_client;
  int seatbelt_fd = seatbelt_client.GetReadFD();
  ASSERT_GE(seatbelt_fd, 0);

  base::LaunchOptions options;
  if (check_initializer_order) {
    options.environment["DYLD_PRINT_INITIALIZERS"] = "1";
  }
  options.fds_to_remap.emplace_back(null_fd.get(), STDOUT_FILENO);
  options.fds_to_remap.emplace_back(write_fd.get(), STDERR_FILENO);
  options.fds_to_remap.emplace_back(seatbelt_fd, seatbelt_fd);

  base::CommandLine command_line(helper_path);
  command_line.AppendSwitchASCII(switches::kProcessType,
                                 switches::kNoOpForTestingProcess);
  command_line.AppendSwitchASCII(sandbox::switches::kSeatbeltClientName,
                                 base::NumberToString(seatbelt_fd));

  base::Process process = base::LaunchProcess(command_line, options);
  ASSERT_TRUE(process.IsValid());

  std::string profile =
      sandbox::policy::GetSandboxProfile(sandbox::mojom::Sandbox::kUtility) +
      "\n(allow file* (subpath \"/private/var/folders\"))";
  sandbox::SandboxSerializer serializer(
      sandbox::SandboxSerializer::Target::kSource);
  serializer.SetProfile(profile);

  int32_t major_version = 0, minor_version = 0, bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&major_version, &minor_version,
                                               &bugfix_version);
  int32_t os_version = major_version * 100 + minor_version;

  EXPECT_TRUE(serializer.SetParameter(
      "EXECUTABLE_PATH",
      sandbox::policy::GetCanonicalPath(command_line.GetProgram()).value()));
  EXPECT_TRUE(serializer.SetBooleanParameter("ENABLE_LOGGING", false));
  EXPECT_TRUE(
      serializer.SetBooleanParameter("DISABLE_SANDBOX_DENIAL_LOGGING", true));
  EXPECT_TRUE(serializer.SetBooleanParameter("ENABLE_DISTRIBUTED_NOTIFICATIONS",
                                             false));
  EXPECT_TRUE(serializer.SetParameter(
      "BUNDLE_PATH",
      sandbox::policy::GetCanonicalPath(base::apple::MainBundlePath())
          .value()));
  EXPECT_TRUE(serializer.SetParameter(
      "BUNDLE_ID", std::string(base::apple::BaseBundleID())));
  EXPECT_TRUE(
      serializer.SetParameter("BROWSER_PID", base::NumberToString(getpid())));
  EXPECT_TRUE(serializer.SetParameter("LOG_FILE_PATH", ""));
  EXPECT_TRUE(
      serializer.SetParameter("OS_VERSION", base::NumberToString(os_version)));
  EXPECT_TRUE(serializer.SetParameter(
      "USER_HOMEDIR_AS_LITERAL",
      sandbox::policy::GetCanonicalPath(base::GetHomeDir()).value()));

#if defined(COMPONENT_BUILD)
  base::FilePath component_path = base::apple::MainBundlePath().Append("..");
  EXPECT_TRUE(serializer.SetParameter(
      "COMPONENT_PATH",
      sandbox::policy::GetCanonicalPath(component_path).value()));
#endif

  std::string error, serialized;
  ASSERT_TRUE(serializer.SerializePolicy(serialized, error)) << error;
  ASSERT_TRUE(seatbelt_client.SendPolicy(serialized));

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

  // Now wait for the process to exit and verify it exited cleanly with code 0.
  int rv = -1;
  ASSERT_TRUE(process.WaitForExit(&rv));
  EXPECT_EQ(rv, 0);

  // On macOS 13, DYLD_PRINT_INITIALIZERS is not set due to the dyld4 bug, so
  // verifying clean process exit above is sufficient.
  if (!check_initializer_order) {
    return;
  }

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
#if defined(ADDRESS_SANITIZER) || defined(UNDEFINED_SANITIZER) || \
    defined(THREAD_SANITIZER)
  allowed_libraries.insert("/usr/lib/libc++abi.dylib");
  allowed_libraries.insert("/usr/lib/libc++.1.dylib");
#endif
  base::FilePath canonical_helper_dir =
      sandbox::policy::GetCanonicalPath(helper_path.DirName());

#if defined(ADDRESS_SANITIZER)
  allowed_libraries.insert(
      dir_exe_path.Append("libclang_rt.asan_osx_dynamic.dylib").value());
  allowed_libraries.insert(
      canonical_helper_dir.Append("libclang_rt.asan_osx_dynamic.dylib")
          .value());
#endif
#if defined(UNDEFINED_SANITIZER)
  allowed_libraries.insert(
      dir_exe_path.Append("libclang_rt.ubsan_osx_dynamic.dylib").value());
  allowed_libraries.insert(
      canonical_helper_dir.Append("libclang_rt.ubsan_osx_dynamic.dylib")
          .value());
#endif
#if defined(THREAD_SANITIZER)
  allowed_libraries.insert(
      dir_exe_path.Append("libclang_rt.tsan_osx_dynamic.dylib").value());
  allowed_libraries.insert(
      canonical_helper_dir.Append("libclang_rt.tsan_osx_dynamic.dylib")
          .value());
#endif
  const std::string kLibaperitifName = "libaperitif.dylib";

  // Check that only allowed libraries or libraries in /usr/lib/system run
  // before libaperitif.
  bool found_libaperitif = false;
  for (size_t i = 0; i < messages.size(); ++i) {
    std::string initializer_path;
    // Continue on the loop if the current message doesn't contain any
    // initializer paths at all.
    if (!re2::RE2::FullMatch(messages[i], pattern, &initializer_path)) {
      continue;
    }
    bool is_libaperitif =
        initializer_path.find(kLibaperitifName) != std::string::npos;

    if (is_libaperitif) {
      found_libaperitif = true;
      break;
    }

    // Only foundational OS libraries (like libSystem) and component
    // dependencies are allowed to run before Aperitif.
    bool is_foundational =
        allowed_libraries.count(initializer_path) ||
        base::StartsWith(initializer_path, "/usr/lib/system/",
                         base::CompareCase::SENSITIVE);
    ASSERT_TRUE(is_foundational)
        << "This initializer ran before " << kLibaperitifName
        << " finished initializing: " << messages[i];
  }

  EXPECT_TRUE(found_libaperitif);
}

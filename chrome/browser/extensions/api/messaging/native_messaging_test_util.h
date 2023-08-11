// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGING_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGING_TEST_UTIL_H_

#include <memory>
#include <string_view>

#include "base/files/scoped_temp_dir.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/test/test_reg_util_win.h"
#else
#include "base/test/scoped_path_override.h"
#endif

namespace extensions {

// Helper class for native messaging tests. When RegisterTestHost() is called it
// creates the following manifest files:
//   kHostName ("com.google.chrome.test.echo") - Echo NM host that runs, see
//       chrome/test/data/native_messaging/native_hosts/echo.py .
//   kBinaryMissingHostName ("com.google.chrome.test.host_binary_missing") -
//        Manifest file that points to a nonexistent host binary.
class ScopedTestNativeMessagingHost {
 public:
  static const char kHostName[];
  static const char kBinaryMissingHostName[];
  static const char kSupportsNativeInitiatedConnectionsHostName[];

#if BUILDFLAG(IS_WIN)
  // When run on Windows, an additional .EXE backed NativeHost is available.
  static const char kHostExeName[];
#endif

  static const char kExtensionId[];

  ScopedTestNativeMessagingHost();

  ScopedTestNativeMessagingHost(const ScopedTestNativeMessagingHost&) = delete;
  ScopedTestNativeMessagingHost& operator=(
      const ScopedTestNativeMessagingHost&) = delete;

  ~ScopedTestNativeMessagingHost();

  void RegisterTestHost(bool user_level);
#if BUILDFLAG(IS_WIN)
  // Register the Windows-only Native Host exe.
  void RegisterTestExeHost(std::string_view filename, bool user_level);
#endif

  const base::FilePath& temp_dir() { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;

#if BUILDFLAG(IS_WIN)
  registry_util::RegistryOverrideManager registry_override_;
#else
  std::unique_ptr<base::ScopedPathOverride> path_override_;
#endif
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGING_TEST_UTIL_H_

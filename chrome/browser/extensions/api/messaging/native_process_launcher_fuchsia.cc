// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_process_launcher.h"

#include "base/notreached.h"

namespace extensions {

// static
base::FilePath NativeProcessLauncher::FindManifest(const std::string& host_name,
                                                   bool allow_user_level_hosts,
                                                   std::string* error_message) {
  *error_message = "Not implemented";
  return base::FilePath();
}

// static
bool NativeProcessLauncher::LaunchNativeProcess(
    const base::CommandLine& command_line,
    base::Process* process,
    base::File* read_file,
    base::File* write_file,
    // This is only relevant on Windows.
    bool native_hosts_executables_launch_directly) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

}  // namespace extensions

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_PEH_LAUNCHER_H_
#define CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_PEH_LAUNCHER_H_

#include "base/files/file_path.h"
#include "base/process/process.h"
#include "base/version.h"

namespace base {
class CommandLine;
struct LaunchOptions;
}  // namespace base

namespace platform_experience {

// Class responsible for locating and launching the platform experience helper
// binary. Overridden in tests to mock process execution.
class PehLauncher {
 public:
  PehLauncher();
  virtual ~PehLauncher();

  PehLauncher(const PehLauncher&) = delete;
  PehLauncher& operator=(const PehLauncher&) = delete;

  // Resolves the path of the Platform Experience Helper executable.
  // Returns an empty path if the binary is not found on disk.
  virtual base::FilePath GetBinaryPath();

  // Verifies the digital signature and authenticity of the binary at `path`.
  // Returns true if the binary is valid and trusted (or in non-official
  // builds).
  virtual bool IsBinaryVerified(const base::FilePath& binary_path);

  // Returns the version of the PEH binary at `peh_binary_path`.
  // Returns an invalid version if the file is missing or has no version info.
  virtual base::Version GetBinaryVersion(const base::FilePath& peh_binary_path);

  // Launches the process specified by `cmd_line` with `options`.
  virtual base::Process LaunchProcess(const base::CommandLine& cmd_line,
                                      const base::LaunchOptions& options);
};

}  // namespace platform_experience

#endif  // CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_PEH_LAUNCHER_H_

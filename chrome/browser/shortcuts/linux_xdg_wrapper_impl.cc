// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/linux_xdg_wrapper_impl.h"

#include <stdlib.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/shell_integration_linux.h"

namespace shortcuts {

LinuxXdgWrapperImpl::LinuxXdgWrapperImpl() = default;
LinuxXdgWrapperImpl::~LinuxXdgWrapperImpl() = default;

int LinuxXdgWrapperImpl::XdgDesktopMenuInstall(
    const base::FilePath& desktop_file) {
  std::vector<std::string> argv;
  argv.push_back("xdg-desktop-menu");
  argv.push_back("install");

  // Always install in user mode, even if someone runs the browser as root
  // (people do that).
  argv.push_back("--mode");
  argv.push_back("user");

  argv.push_back(desktop_file.value());
  int exit_code = EXIT_SUCCESS;
  shell_integration_linux::LaunchXdgUtility(argv, &exit_code);
  return exit_code;
}

}  // namespace shortcuts

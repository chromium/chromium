// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHORTCUTS_SHORTCUT_CREATION_TEST_SUPPORT_LINUX_H_
#define CHROME_BROWSER_SHORTCUTS_SHORTCUT_CREATION_TEST_SUPPORT_LINUX_H_

#include <string>
#include <vector>

namespace shortcuts::internal {

// This method is roughly the reverse of `QuoteCommandLineForDesktopFileExec` in
// shell_integration_linux.cc. It splits up the given string `s` command line
// into its individual arguments allowing a `base::CommandLine` to be created
// from the Exec value in a .desktop file.
// This method assumes that its input is a well-formed command line, and will
// CHECK-fail if that is not the case.
std::vector<std::string> ParseDesktopExecForCommandLine(const std::string& s);

}  // namespace shortcuts::internal

#endif  // CHROME_BROWSER_SHORTCUTS_SHORTCUT_CREATION_TEST_SUPPORT_LINUX_H_

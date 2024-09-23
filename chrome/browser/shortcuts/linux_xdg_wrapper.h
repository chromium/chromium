// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHORTCUTS_LINUX_XDG_WRAPPER_H_
#define CHROME_BROWSER_SHORTCUTS_LINUX_XDG_WRAPPER_H_

namespace base {
class FilePath;
}  // namespace base

namespace shortcuts {

// This class wraps calling the Linux 'xdg' command-line functionality needed
// for shortcut creation, allowing tests to not actually impact the operating
// system.
// TODO(crbug.com/337934019): Revisit this once the feature is fully implemented
// and evaluate if this should become a callback.
class LinuxXdgWrapper {
 public:
  virtual ~LinuxXdgWrapper() = default;

  // Calls 'xdg-desktop-menu' to install the given '.desktop' file in the
  // desktop menu. Returns the error code from the linux system, which is
  // EXIT_SUCCESS on success.
  virtual int XdgDesktopMenuInstall(const base::FilePath& desktop_file) = 0;
};

}  // namespace shortcuts

#endif  // CHROME_BROWSER_SHORTCUTS_LINUX_XDG_WRAPPER_H_

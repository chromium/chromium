// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHORTCUTS_SHORTCUT_CREATOR_LINUX_TEST_SUPPORT_H_
#define CHROME_BROWSER_SHORTCUTS_SHORTCUT_CREATOR_LINUX_TEST_SUPPORT_H_

#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/shortcuts/linux_xdg_wrapper.h"

namespace shortcuts {

// This class fakes the LinuxXdgWrapper functionality to allow unit testing
// without modifying the local operating system of file system. It also includes
// common testing functions to help unit tests and browser tests check the saved
// shortcut state.
//
// This also includes overriding the base::DIR_USER_DESKTOP directory path,
// which is where the desktop file will be stored when created, referenced by
// `GetInstalls()`.
// TODO(crbug.com/337934019): Revisit this once the feature is fully implemented
// with browser tests and evaluate if this should become a callback, or if it's
// useful to put more testing utilities on.
class ShortcutCreatorLinuxTestSupport : public LinuxXdgWrapper {
 public:
  ShortcutCreatorLinuxTestSupport();
  ~ShortcutCreatorLinuxTestSupport() override;
  ShortcutCreatorLinuxTestSupport(const ShortcutCreatorLinuxTestSupport&) =
      delete;
  ShortcutCreatorLinuxTestSupport& operator=(
      const ShortcutCreatorLinuxTestSupport&) = delete;

  // Desktop menu files installed.
  const std::vector<base::FilePath>& GetInstalls() const { return installs_; }

  // LinuxXdgWrapper implementation:
  int XdgDesktopMenuInstall(const base::FilePath& desktop_file) override;

 private:
  base::ScopedPathOverride desktop_override_{base::DIR_USER_DESKTOP};
  std::vector<base::FilePath> installs_;
};

}  // namespace shortcuts

#endif  // CHROME_BROWSER_SHORTCUTS_SHORTCUT_CREATOR_LINUX_TEST_SUPPORT_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHORTCUTS_LINUX_XDG_WRAPPER_IMPL_H_
#define CHROME_BROWSER_SHORTCUTS_LINUX_XDG_WRAPPER_IMPL_H_

#include "chrome/browser/shortcuts/linux_xdg_wrapper.h"

namespace base {
class FilePath;
}  // namespace base

namespace shortcuts {

// The 'real' implementation of LinuxXdgWrapper which calls the linux `xdg`
// command line utility for shortcut creation.
// TODO(crbug.com/337934019): Revisit this once the feature is fully implemented
// and evaluate if this should become a callback.
class LinuxXdgWrapperImpl : public LinuxXdgWrapper {
 public:
  LinuxXdgWrapperImpl();
  ~LinuxXdgWrapperImpl() override;

  int XdgDesktopMenuInstall(const base::FilePath& desktop_file) override;
};

}  // namespace shortcuts

#endif  // CHROME_BROWSER_SHORTCUTS_LINUX_XDG_WRAPPER_IMPL_H_

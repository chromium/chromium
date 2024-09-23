// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHORTCUTS_FAKE_LINUX_XDG_WRAPPER_H_
#define CHROME_BROWSER_SHORTCUTS_FAKE_LINUX_XDG_WRAPPER_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/shortcuts/linux_xdg_wrapper.h"

namespace shortcuts {

// This class fakes the LinuxXdgWrapper functionality to allow unit testing
// without modifying the local operating system of file system.
class FakeLinuxXdgWrapper : public LinuxXdgWrapper {
 public:
  FakeLinuxXdgWrapper();
  ~FakeLinuxXdgWrapper() override;
  FakeLinuxXdgWrapper(const FakeLinuxXdgWrapper&) = delete;
  FakeLinuxXdgWrapper& operator=(const FakeLinuxXdgWrapper&) = delete;

  // Desktop menu files installed.
  const std::vector<base::FilePath>& GetInstalls() const { return installs_; }

  // LinuxXdgWrapper implementation:
  int XdgDesktopMenuInstall(const base::FilePath& desktop_file) override;

 private:
  std::vector<base::FilePath> installs_;
};

}  // namespace shortcuts

#endif  // CHROME_BROWSER_SHORTCUTS_FAKE_LINUX_XDG_WRAPPER_H_

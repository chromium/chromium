// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/shortcut_creator_linux_test_support.h"

#include <stdlib.h>

#include "base/files/file_path.h"

namespace shortcuts {

ShortcutCreatorLinuxTestSupport::ShortcutCreatorLinuxTestSupport() = default;
ShortcutCreatorLinuxTestSupport::~ShortcutCreatorLinuxTestSupport() = default;

int ShortcutCreatorLinuxTestSupport::XdgDesktopMenuInstall(
    const base::FilePath& desktop_file) {
  installs_.push_back(desktop_file);
  return EXIT_SUCCESS;
}

}  // namespace shortcuts

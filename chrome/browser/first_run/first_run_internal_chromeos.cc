// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run_internal.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/installer/util/initial_preferences.h"

namespace first_run::internal {

bool IsOrganicFirstRun() {
  // We treat all installs as organic.
  return true;
}

base::FilePath InitialPrefsPath() {
  // The standard location of the initial prefs is next to the chrome binary.
  base::FilePath dir_exe;
  if (!base::PathService::Get(base::DIR_EXE, &dir_exe)) {
    return base::FilePath();
  }

  return installer::InitialPreferences::Path(dir_exe);
}

void DoPostImportPlatformSpecificTasks() {
  // Nothing to do.
}

bool ShowPostInstallEULAIfNeeded(installer::InitialPreferences* install_prefs) {
  // Just continue. The EULA is only used on Windows and Linux.
  return true;
}

}  // namespace first_run::internal

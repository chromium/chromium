// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run_internal.h"

#include "base/files/file_path.h"

namespace first_run {
namespace internal {

bool IsOrganicFirstRun() {
  // We treat all installs as organic.
  return true;
}

base::FilePath InitialPrefsPath() {
  // Initial preferences are not used on Fuchsia.
  return base::FilePath();
}

void DoPostImportPlatformSpecificTasks() {}

bool ShowPostInstallEULAIfNeeded(installer::InitialPreferences* install_prefs) {
  // The EULA is only handled on Windows.
  return true;
}

}  // namespace internal
}  // namespace first_run

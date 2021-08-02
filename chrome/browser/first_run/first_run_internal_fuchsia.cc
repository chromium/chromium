// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run_internal.h"

#include "base/files/file_path.h"
#include "base/notreached.h"

namespace first_run {
namespace internal {

bool IsOrganicFirstRun() {
  // We treat all installs as organic.
  return true;
}

base::FilePath InitialPrefsPath() {
  // TODO(crbug.com/1234776)
  NOTIMPLEMENTED_LOG_ONCE();
  return base::FilePath();
}

void DoPostImportPlatformSpecificTasks(Profile* profile) {
  // TODO(crbug.com/1234776)
  NOTIMPLEMENTED_LOG_ONCE();
}

bool ShowPostInstallEULAIfNeeded(installer::InitialPreferences* install_prefs) {
  // The EULA is only handled on Windows.
  return true;
}

}  // namespace internal
}  // namespace first_run

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run_internal.h"

namespace first_run {
namespace internal {

void DoPostImportPlatformSpecificTasks() {
  // Nothing to do.
}

bool ShowPostInstallEULAIfNeeded(installer::InitialPreferences* install_prefs) {
  // Just continue. The EULA is only used on Windows.
  return true;
}

}  // namespace internal
}  // namespace first_run

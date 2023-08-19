// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run_internal.h"

#include "chrome/browser/mac/initial_prefs.h"

namespace first_run::internal {

base::FilePath InitialPrefsPath() {
  return initial_prefs::InitialPrefsPath();
}

}  // namespace first_run::internal

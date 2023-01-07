// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/upgrade_util.h"

#include "base/notreached.h"

namespace upgrade_util {

bool RelaunchChromeBrowserImpl(const base::CommandLine& command_line) {
  // TODO(crbug.com/1234776)
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool IsUpdatePendingRestart() {
  // TODO(crbug.com/1234776)
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

}  // namespace upgrade_util

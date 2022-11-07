// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEADLESS_HEADLESS_MODE_UTIL_H_
#define CHROME_BROWSER_HEADLESS_HEADLESS_MODE_UTIL_H_

#include "base/command_line.h"

namespace headless {

// Returns positive if new headless mode is in effect. The new headless mode
// is Chrome browser running without any visible UI.
bool IsHeadlessMode();

// Returns positive if old headless mode is in effect. The old headless mode
// is a minimalistic browser implementation found in //headless which lacks
// most of the full fledged Chrome browser functionality.
bool IsOldHeadlessMode();

// Adds command line switches necessary for the native headless mode.
void SetUpCommandLine(const base::CommandLine* command_line);

}  // namespace headless

#endif  // CHROME_BROWSER_HEADLESS_HEADLESS_MODE_UTIL_H_

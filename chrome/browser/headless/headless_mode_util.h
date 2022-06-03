// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEADLESS_HEADLESS_MODE_UTIL_H_
#define CHROME_BROWSER_HEADLESS_HEADLESS_MODE_UTIL_H_

#include "base/command_line.h"

namespace headless {

// Returns positive if Chrome native headless mode is in effect.
bool IsChromeNativeHeadless();

// Adds command line switches necessary for the native headless mode.
void SetUpCommandLine(const base::CommandLine* command_line);

}  // namespace headless

#endif  // CHROME_BROWSER_HEADLESS_HEADLESS_MODE_UTIL_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_mode_util.h"

#ifndef CHROME_BROWSER_HEADLESS_HEADLESS_MODE_PLATFORM_H_
#define CHROME_BROWSER_HEADLESS_HEADLESS_MODE_PLATFORM_H_

namespace headless {

// Performs platform specific headless mode initialization.
void InitializePlatform();

}  // namespace headless

#endif  // CHROME_BROWSER_HEADLESS_HEADLESS_MODE_PLATFORM_H_

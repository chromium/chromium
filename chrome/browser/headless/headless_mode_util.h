// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEADLESS_HEADLESS_MODE_UTIL_H_
#define CHROME_BROWSER_HEADLESS_HEADLESS_MODE_UTIL_H_

#include <memory>

#include "base/command_line.h"

namespace headless {

// Represents opaque headless mode state.
class HeadlessModeHandle {
 public:
  HeadlessModeHandle() = default;
  virtual ~HeadlessModeHandle() = default;
};

// Returns positive if new headless mode is in effect. The new headless mode
// is Chrome browser running without any visible UI.
bool IsHeadlessMode();

// Returns positive if old headless mode is in effect. The old headless mode
// is a minimalistic browser implementation found in //headless which lacks
// most of the full fledged Chrome browser functionality.
bool IsOldHeadlessMode();

// Returns positive if headless mode can access any URL whose scheme is
// chrome://.
bool IsChromeSchemeUrlAllowed();

// Initializes headless mode returning a handle that would clean up the state
// upon destruction.
std::unique_ptr<HeadlessModeHandle> InitHeadlessMode();

}  // namespace headless

#endif  // CHROME_BROWSER_HEADLESS_HEADLESS_MODE_UTIL_H_

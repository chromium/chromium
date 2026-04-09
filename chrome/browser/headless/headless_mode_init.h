// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEADLESS_HEADLESS_MODE_INIT_H_
#define CHROME_BROWSER_HEADLESS_HEADLESS_MODE_INIT_H_

#include <memory>
#include <string>

#include "base/types/expected.h"
#include "chrome/browser/headless/headless_mode_util.h"

namespace headless {

// Represents opaque headless mode state.
class HeadlessModeHandle {
 public:
  HeadlessModeHandle() = default;
  virtual ~HeadlessModeHandle() = default;
};

// Initializes headless mode returning a handle that would clean up the state
// upon destruction or a meaningful error message.
base::expected<std::unique_ptr<HeadlessModeHandle>, std::string>
InitHeadlessMode();

}  // namespace headless

#endif  // CHROME_BROWSER_HEADLESS_HEADLESS_MODE_INIT_H_

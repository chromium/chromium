// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_mode_browsertest_utils.h"

#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"

namespace headless::test {

void ToggleFullscreenModeSync(Browser* browser) {
  FullscreenNotificationObserver observer(browser);
  chrome::ToggleFullscreenMode(browser);
  observer.Wait();
}

}  // namespace headless::test

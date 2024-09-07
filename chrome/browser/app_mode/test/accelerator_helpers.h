// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APP_MODE_TEST_ACCELERATOR_HELPERS_H_
#define CHROME_BROWSER_APP_MODE_TEST_ACCELERATOR_HELPERS_H_

#include "chrome/browser/ui/browser.h"

// Presses Ctrl + W using `browser`'s view as the accelerator target.
[[nodiscard]] bool PressCloseTabAccelerator(Browser* browser);

// Presses Ctrl + Shift + W using `browser`'s view as the accelerator target.
[[nodiscard]] bool PressCloseWindowAccelerator(Browser* browser);

#endif  // CHROME_BROWSER_APP_MODE_TEST_ACCELERATOR_HELPERS_H_

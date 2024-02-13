// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEADLESS_HEADLESS_MODE_BROWSERTEST_UTILS_H_
#define CHROME_BROWSER_HEADLESS_HEADLESS_MODE_BROWSERTEST_UTILS_H_

#include "ui/gfx/geometry/rect.h"

namespace views {
class Widget;
}

namespace headless::test {

// Returns the visibility state of the platform window associated with the
// widget. This method has platform specific implementations.
bool IsPlatformWindowVisible(views::Widget* widget);

// Returns the expected bounds of the platform window associated with the
// widget. This method has platform specific implementations.
gfx::Rect GetPlatformWindowExpectedBounds(views::Widget* widget);

}  // namespace headless::test

#endif  // CHROME_BROWSER_HEADLESS_HEADLESS_MODE_BROWSERTEST_UTILS_H_

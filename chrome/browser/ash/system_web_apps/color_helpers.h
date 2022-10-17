// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_COLOR_HELPERS_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_COLOR_HELPERS_H_

#include "third_party/skia/include/core/SkColor.h"

namespace ash {
// Returns the theme color that system web apps should use, as reported by
// the operating system theme.
SkColor GetSystemThemeColor();

// Returns the background color that system web apps should use, as reported
// by the operating system theme.
SkColor GetSystemBackgroundColor();
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_COLOR_HELPERS_H_

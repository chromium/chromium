// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_THEME_UTIL_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_THEME_UTIL_H_

#include "third_party/skia/include/core/SkColor.h"

class Profile;

namespace ui {
class ColorProvider;
}

namespace glic {

// Returns the background color for Glic (kColorGlicBackground).
SkColor GetGlicBackgroundColor(Profile* profile,
                               const ui::ColorProvider& color_provider);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_THEME_UTIL_H_

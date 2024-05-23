// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_CHROME_COLORS_CHROME_COLORS_UTIL_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_CHROME_COLORS_CHROME_COLORS_UTIL_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/mojom/themes.mojom.h"

namespace chrome_colors {

// These constants have to match the values of ChromeColorsInfo and
// DynamicChromeColorsInfo in enums.xml.
inline constexpr int kDefaultColorId = -1;
inline constexpr int kOtherColorId = 0;
inline constexpr int kOtherDynamicColorId = 0;
inline constexpr int kGrayscaleDynamicColorId = 1;

void RecordColorOnLoadHistogram(SkColor color);
void RecordDynamicColorOnLoadHistogramForGrayscale();
void RecordDynamicColorOnLoadHistogram(SkColor color,
                                       ui::mojom::BrowserColorVariant variant);

// Returns id for the given `color` if it is in the predefined set, and
// `kOtherColorId` otherwise.
// Do not confuse these integers (including the dynamically generated ones) with
// the color IDs from the color pipeline. These integers represent fixed color
// schemes, see the enums.xml file for more details.
int GetChromeColorsInfo(SkColor color);

}  // namespace chrome_colors

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_CHROME_COLORS_CHROME_COLORS_UTIL_H_

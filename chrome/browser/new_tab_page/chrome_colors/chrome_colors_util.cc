// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/chrome_colors/chrome_colors_util.h"

#include <iterator>

#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/new_tab_page/chrome_colors/generated_colors_info.h"
#include "chrome/browser/ui/webui/cr_components/theme_color_picker/customize_chrome_colors.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/mojom/themes.mojom.h"

namespace chrome_colors {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ChromeColorType {
  kChromeColor = 0,
  kDynamicChromeColor = 1,
  kMaxValue = kDynamicChromeColor,
};

void RecordChromeColorsColorType(ChromeColorType type) {
  base::UmaHistogramEnumeration("ChromeColors.ColorType", type);
}

void RecordChromeColorsDynamicColor(int color_id) {
  base::UmaHistogramExactLinear(
      "ChromeColors.DynamicColorOnLoad", color_id,
      base::ranges::max_element(kDynamicCustomizeChromeColors, {},
                                &DynamicColorInfo::id)
          ->id);
  RecordChromeColorsColorType(ChromeColorType::kDynamicChromeColor);
}

int GetDynamicColorId(const SkColor color,
                      ui::mojom::BrowserColorVariant variant) {
  auto it = base::ranges::find_if(kDynamicCustomizeChromeColors,
                                  [&](const DynamicColorInfo& dynamic_color) {
                                    return dynamic_color.color == color &&
                                           dynamic_color.variant == variant;
                                  });
  return it == kDynamicCustomizeChromeColors.end() ? kOtherDynamicColorId
                                                   : it->id;
}

}  // namespace

void RecordColorOnLoadHistogram(SkColor color) {
  base::UmaHistogramExactLinear("ChromeColors.ColorOnLoad",
                                GetChromeColorsInfo(color), kNumColorsInfo);
  RecordChromeColorsColorType(ChromeColorType::kChromeColor);
}

void RecordDynamicColorOnLoadHistogramForGrayscale() {
  RecordChromeColorsDynamicColor(kGrayscaleDynamicColorId);
}

void RecordDynamicColorOnLoadHistogram(SkColor color,
                                       ui::mojom::BrowserColorVariant variant) {
  RecordChromeColorsDynamicColor(GetDynamicColorId(color, variant));
}

int GetChromeColorsInfo(SkColor color) {
  const auto it = base::ranges::find(chrome_colors::kGeneratedColorsInfo, color,
                                     &chrome_colors::ColorInfo::color);
  return it == std::end(chrome_colors::kGeneratedColorsInfo) ? kOtherColorId
                                                             : it->id;
}

}  // namespace chrome_colors

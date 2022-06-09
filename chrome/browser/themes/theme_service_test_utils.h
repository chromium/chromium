// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_SERVICE_TEST_UTILS_H_
#define CHROME_BROWSER_THEMES_THEME_SERVICE_TEST_UTILS_H_

#include <ostream>
#include <string>

#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/theme_provider.h"

namespace theme_service::test {

#define E(color_id, theme_property_id, ...) theme_property_id,
#define E_CPONLY(color_id, ...)
static constexpr int kTestColorIds[] = {CHROME_COLOR_IDS};
#undef E
#undef E_CPONLY

static constexpr const auto kColorTolerances = base::MakeFixedFlatMap<int, int>(
    {{ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_SECONDARY, 1},
     {ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_SECONDARY_SELECTED, 1},
     {ThemeProperties::COLOR_STATUS_BUBBLE_INACTIVE, 1},
     {ThemeProperties::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE, 1},
     {ThemeProperties::COLOR_TAB_GROUP_BOOKMARK_BAR_ORANGE, 1},
     {ThemeProperties::COLOR_TAB_STROKE_FRAME_INACTIVE, 1},
     {ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR_FRAME_INACTIVE, 1},
     {ThemeProperties::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INACTIVE, 1}});

// Struct to distinguish SkColor (aliased to uint32_t) for printing.
struct PrintableSkColor {
  bool operator==(const PrintableSkColor& other) const;
  bool operator!=(const PrintableSkColor& other) const;

  const SkColor color;
};

std::ostream& operator<<(std::ostream& os, PrintableSkColor printable_color);

std::string ColorIdToString(int id);

std::pair<PrintableSkColor, PrintableSkColor> GetOriginalAndRedirected(
    const ui::ThemeProvider& theme_provider,
    int color_id);

void TestOriginalAndRedirectedColorMatched(
    const ui::ThemeProvider& theme_provider,
    int color_id,
    const std::string& error_message);

}  // namespace theme_service::test

#endif  // CHROME_BROWSER_THEMES_THEME_SERVICE_TEST_UTILS_H_

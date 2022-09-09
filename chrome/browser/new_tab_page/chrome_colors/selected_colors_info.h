// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_CHROME_COLORS_SELECTED_COLORS_INFO_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_CHROME_COLORS_SELECTED_COLORS_INFO_H_

#include <stdint.h>

#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"

namespace chrome_colors {

struct ColorInfo {
  constexpr ColorInfo(int id, SkColor color, int label_id)
      : id(id), color(color), label_id(label_id) {}
  int id;
  SkColor color;
  int label_id;
};

// List of preselected colors to show in Chrome Colors menu. This array should
// always be in sync with ChromeColorsInfo in enums.xml.
constexpr ColorInfo kSelectedColorsInfo[] = {
    // 0  - reserved for any color not in this set.
    ColorInfo(1, SkColorSetRGB(239, 235, 233), IDS_NTP_COLORS_WARM_GREY),
    ColorInfo(2, SkColorSetRGB(120, 127, 145), IDS_NTP_COLORS_COOL_GREY),
    ColorInfo(3, SkColorSetRGB(55, 71, 79), IDS_NTP_COLORS_MIDNIGHT_BLUE),
    ColorInfo(4, SkColorSetRGB(0, 0, 0), IDS_NTP_COLORS_BLACK),
    ColorInfo(5, SkColorSetRGB(252, 219, 201), IDS_NTP_COLORS_BEIGE_AND_WHITE),
    ColorInfo(6, SkColorSetRGB(255, 249, 228), IDS_NTP_COLORS_YELLOW_AND_WHITE),
    ColorInfo(7, SkColorSetRGB(203, 233, 191), IDS_NTP_COLORS_GREEN_AND_WHITE),
    ColorInfo(8,
              SkColorSetRGB(221, 244, 249),
              IDS_NTP_COLORS_LIGHT_TEAL_AND_WHITE),
    ColorInfo(9,
              SkColorSetRGB(233, 212, 255),
              IDS_NTP_COLORS_LIGHT_PURPLE_AND_WHITE),
    ColorInfo(10, SkColorSetRGB(249, 226, 237), IDS_NTP_COLORS_PINK_AND_WHITE),
    ColorInfo(11, SkColorSetRGB(227, 171, 154), IDS_NTP_COLORS_BEIGE),
    ColorInfo(12, SkColorSetRGB(255, 171, 64), IDS_NTP_COLORS_ORANGE),
    ColorInfo(13, SkColorSetRGB(67, 160, 71), IDS_NTP_COLORS_LIGHT_GREEN),
    ColorInfo(14, SkColorSetRGB(25, 157, 169), IDS_NTP_COLORS_LIGHT_TEAL),
    ColorInfo(15, SkColorSetRGB(93, 147, 228), IDS_NTP_COLORS_LIGHT_BLUE),
    ColorInfo(16, SkColorSetRGB(255, 174, 189), IDS_NTP_COLORS_PINK),
    ColorInfo(17, SkColorSetRGB(189, 22, 92), IDS_NTP_COLORS_DARK_PINK_AND_RED),
    ColorInfo(18,
              SkColorSetRGB(183, 28, 28),
              IDS_NTP_COLORS_DARK_RED_AND_ORANGE),
    ColorInfo(19, SkColorSetRGB(46, 125, 50), IDS_NTP_COLORS_DARK_GREEN),
    ColorInfo(20, SkColorSetRGB(0, 110, 120), IDS_NTP_COLORS_DARK_TEAL),
    ColorInfo(21, SkColorSetRGB(21, 101, 192), IDS_NTP_COLORS_DARK_BLUE),
    ColorInfo(22, SkColorSetRGB(91, 54, 137), IDS_NTP_COLORS_DARK_PURPLE),
};

}  // namespace chrome_colors

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_CHROME_COLORS_SELECTED_COLORS_INFO_H_

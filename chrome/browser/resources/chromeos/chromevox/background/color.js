// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides color matching services for ChromeVox.
 */

goog.provide('Color');

/**
 * Returns a string representation of a color.
 * @param {number|undefined} color The argb value represented as an integer.
 * @return {string}
 */
Color.getColorDescription = function(color) {
  if (!color) {
    return '';
  }
  // Convert to unsigned integer.
  color = color >>> 0;
  // The following 24 bits represent the rgb value. Filter out first 8 bits.
  var rgb = color & 0x00ffffff;
  var optSubs =
      [Color.findClosestMatchingColor(rgb), Color.getOpacityPercentage(color)];
  return Msgs.getMsg('color_description', optSubs);
};

/**
 * Extracts the opacity of the color, which is encoded within the first 8 bits.
 * @param {number} color An integer representation of a color.
 * @return {number}
 */
Color.getOpacityPercentage = function(color) {
  return Math.round(((color >>> 24) / 256) * 100);
};

/**
 * Finds the most similar stored color given an rgb value.
 * @param {number} target The rgb value as an integer.
 * @return {string}
 */
Color.findClosestMatchingColor = function(target) {
  var bestMessageId;
  var bestDistance = Number.MAX_VALUE;

  Color.ColorObjectArray.forEach(function(obj) {
    var val = obj.value;
    var distance = Color.findDistance(target, val);
    if (distance < bestDistance) {
      bestMessageId = obj.colorMessageId;
      bestDistance = distance;
    }
  });
  // Do not report color if most similar color is too inaccurate.
  if (bestDistance > Color.DISTANCE_THRESHOLD) {
    return '';
  }
  return Msgs.getMsg(bestMessageId);
};

/**
 * Calculates the distance between two 3-D points, encoded as numbers,
 * that represent colors.
 * The first 8 bits are unused as they have either been shifted off or are
 * simply filled by zeros. The x component is designated by the second
 * 8 bits. The y component is designated by the third 8 bits.
 * The z component is designated by the last 8 bits.
 * @param {number} firstColor
 * @param {number} secondColor
 * @return {number}
 */
Color.findDistance = function(firstColor, secondColor) {
  // Extract x, y, and z components.
  var firstColorX = (firstColor & 0xff0000) >> 16;
  var firstColorY = (firstColor & 0x00ff00) >> 8;
  var firstColorZ = (firstColor & 0x0000ff);
  var secondColorX = (secondColor & 0xff0000) >> 16;
  var secondColorY = (secondColor & 0x00ff00) >> 8;
  var secondColorZ = (secondColor & 0x0000ff);

  return Math.pow(secondColorX - firstColorX, 2) +
      Math.pow(secondColorY - firstColorY, 2) +
      Math.pow(secondColorZ - firstColorZ, 2);
};

/**
 * The distance between black and dark grey is the threshold.
 * 0x000000 = Black.
 * 0x282828 = Dark Grey. This value was chosen somewhat arbitrarily. It encodes
 * a shade of grey that could be visibly identified as black.
 * @const {number}
 */
Color.DISTANCE_THRESHOLD = Color.findDistance(0X000000, 0X282828);

/**
 * Holds objects that contain hexadecimal RGB values of colors and their
 * corresponding ChromeVox message IDs.
 * @private {!Array<{colorMessageId: string, value: number}>}
 * Obtained from url: https://www.w3schools.com/lib/w3color.js
 */
Color.ColorObjectArray = [
  {'value': 0x0, 'colorMessageId': 'color_black'},
  {'value': 0x6400, 'colorMessageId': 'color_dark_green'},
  {'value': 0x8000, 'colorMessageId': 'color_green'},
  {'value': 0x800080, 'colorMessageId': 'color_purple'},
  {'value': 0xb8860b, 'colorMessageId': 'color_dark_golden_rod'},
  {'value': 0xfffacd, 'colorMessageId': 'color_lemon_chiffon'},
  {'value': 0xa0522d, 'colorMessageId': 'color_sienna'},
  {'value': 0xffa500, 'colorMessageId': 'color_orange'},
  {'value': 0x8b4513, 'colorMessageId': 'color_saddle_brown'},
  {'value': 0xffff, 'colorMessageId': 'color_cyan'},
  {'value': 0xadff2f, 'colorMessageId': 'color_green_yellow'},
  {'value': 0xd2691e, 'colorMessageId': 'color_chocolate'},
  {'value': 0x800000, 'colorMessageId': 'color_maroon'},
  {'value': 0xdaa520, 'colorMessageId': 'color_golden_rod'},
  {'value': 0x228b22, 'colorMessageId': 'color_forest_green'},
  {'value': 0x6b8e23, 'colorMessageId': 'color_olive_drab'},
  {'value': 0xfffff0, 'colorMessageId': 'color_ivory'},
  {'value': 0xf5f5dc, 'colorMessageId': 'color_beige'},
  {'value': 0xa52a2a, 'colorMessageId': 'color_brown'},
  {'value': 0x9acd32, 'colorMessageId': 'color_yellow_green'},
  {'value': 0xff4500, 'colorMessageId': 'color_orange_red'},
  {'value': 0x556b2f, 'colorMessageId': 'color_dark_olive_green'},
  {'value': 0x32cd32, 'colorMessageId': 'color_lime_green'},
  {'value': 0xff00, 'colorMessageId': 'color_lime'},
  {'value': 0xeee8aa, 'colorMessageId': 'color_pale_golden_rod'},
  {'value': 0xff69b4, 'colorMessageId': 'color_hot_pink'},
  {'value': 0xdc143c, 'colorMessageId': 'color_crimson'},
  {'value': 0xb0e0e6, 'colorMessageId': 'color_powder_blue'},
  {'value': 0x808000, 'colorMessageId': 'color_olive'},
  {'value': 0xffffe0, 'colorMessageId': 'color_light_yellow'},
  {'value': 0xfaf0e6, 'colorMessageId': 'color_linen'},
  {'value': 0x8b, 'colorMessageId': 'color_dark_blue'},
  {'value': 0xf8f8ff, 'colorMessageId': 'color_ghost_white'},
  {'value': 0xff6347, 'colorMessageId': 'color_tomato'},
  {'value': 0xf0e68c, 'colorMessageId': 'color_khaki'},
  {'value': 0x2f4f4f, 'colorMessageId': 'color_dark_slate_grey'},
  {'value': 0xff7f50, 'colorMessageId': 'color_coral'},
  {'value': 0xf5fffa, 'colorMessageId': 'color_mint_cream'},
  {'value': 0x8080, 'colorMessageId': 'color_teal'},
  {'value': 0x8b008b, 'colorMessageId': 'color_dark_magenta'},
  {'value': 0xffa07a, 'colorMessageId': 'color_light_salmon'},
  {'value': 0x2e8b57, 'colorMessageId': 'color_sea_green'},
  {'value': 0xff0000, 'colorMessageId': 'color_red'},
  {'value': 0xbc8f8f, 'colorMessageId': 'color_rosy_brown'},
  {'value': 0xcd5c5c, 'colorMessageId': 'color_indian_red'},
  {'value': 0xd3d3d3, 'colorMessageId': 'color_light_grey'},
  {'value': 0xf4a460, 'colorMessageId': 'color_sandy_brown'},
  {'value': 0x90ee90, 'colorMessageId': 'color_light_green'},
  {'value': 0xadd8e6, 'colorMessageId': 'color_light_blue'},
  {'value': 0xff8c00, 'colorMessageId': 'color_dark_orange'},
  {'value': 0x696969, 'colorMessageId': 'color_dim_grey'},
  {'value': 0xffebcd, 'colorMessageId': 'color_blanched_almond'},
  {'value': 0xbdb76b, 'colorMessageId': 'color_dark_khaki'},
  {'value': 0xff00ff, 'colorMessageId': 'color_magenta'},
  {'value': 0x191970, 'colorMessageId': 'color_midnight_blue'},
  {'value': 0x3cb371, 'colorMessageId': 'color_medium_sea_green'},
  {'value': 0xfa8072, 'colorMessageId': 'color_salmon'},
  {'value': 0xff1493, 'colorMessageId': 'color_deep_pink'},
  {'value': 0xe9967a, 'colorMessageId': 'color_dark_salmon'},
  {'value': 0xcd853f, 'colorMessageId': 'color_peru'},
  {'value': 0xff7f, 'colorMessageId': 'color_spring_green'},
  {'value': 0x80, 'colorMessageId': 'color_navy'},
  {'value': 0xf08080, 'colorMessageId': 'color_light_coral'},
  {'value': 0x4b0082, 'colorMessageId': 'color_indigo'},
  {'value': 0xffffff, 'colorMessageId': 'color_white'},
  {'value': 0xc71585, 'colorMessageId': 'color_medium_violet_red'},
  {'value': 0xdeb887, 'colorMessageId': 'color_burly_wood'},
  {'value': 0xe6e6fa, 'colorMessageId': 'color_lavender'},
  {'value': 0x483d8b, 'colorMessageId': 'color_dark_slate_blue'},
  {'value': 0xd2b48c, 'colorMessageId': 'color_tan'},
  {'value': 0x8fbc8f, 'colorMessageId': 'color_dark_sea_green'},
  {'value': 0x708090, 'colorMessageId': 'color_slate_grey'},
  {'value': 0xdb7093, 'colorMessageId': 'color_pale_violet_red'},
  {'value': 0xfff8dc, 'colorMessageId': 'color_cornsilk'},
  {'value': 0xafeeee, 'colorMessageId': 'color_pale_turquoise'},
  {'value': 0x778899, 'colorMessageId': 'color_light_slate_grey'},
  {'value': 0x98fb98, 'colorMessageId': 'color_pale_green'},
  {'value': 0x663399, 'colorMessageId': 'color_rebecca_purple'},
  {'value': 0xfa9a, 'colorMessageId': 'color_medium_spring_green'},
  {'value': 0xffc0cb, 'colorMessageId': 'color_pink'},
  {'value': 0x5f9ea0, 'colorMessageId': 'color_cadet_blue'},
  {'value': 0x808080, 'colorMessageId': 'color_grey'},
  {'value': 0xee82ee, 'colorMessageId': 'color_violet'},
  {'value': 0xa9a9a9, 'colorMessageId': 'color_dark_grey'},
  {'value': 0x20b2aa, 'colorMessageId': 'color_light_sea_green'},
  {'value': 0x8b8b, 'colorMessageId': 'color_dark_cyan'},
  {'value': 0xffdead, 'colorMessageId': 'color_navajo_white'},
  {'value': 0xf0f8ff, 'colorMessageId': 'color_alice_blue'},
  {'value': 0xfffaf0, 'colorMessageId': 'color_floral_white'},
  {'value': 0xffe4e1, 'colorMessageId': 'color_misty_rose'},
  {'value': 0xf5deb3, 'colorMessageId': 'color_wheat'},
  {'value': 0x4682b4, 'colorMessageId': 'color_steel_blue'},
  {'value': 0xffe4b5, 'colorMessageId': 'color_moccasin'},
  {'value': 0xffdab9, 'colorMessageId': 'color_peach_puff'},
  {'value': 0xffd700, 'colorMessageId': 'color_gold'},
  {'value': 0xfff0f5, 'colorMessageId': 'color_lavender_blush'},
  {'value': 0xc0c0c0, 'colorMessageId': 'color_silver'},
  {'value': 0xffb6c1, 'colorMessageId': 'color_light_pink'},
  {'value': 0xf0ffff, 'colorMessageId': 'color_azure'},
  {'value': 0xffe4c4, 'colorMessageId': 'color_bisque'},
  {'value': 0x9932cc, 'colorMessageId': 'color_dark_orchid'},
  {'value': 0xfdf5e6, 'colorMessageId': 'color_old_lace'},
  {'value': 0x48d1cc, 'colorMessageId': 'color_medium_turquoise'},
  {'value': 0x6a5acd, 'colorMessageId': 'color_slate_blue'},
  {'value': 0xcd, 'colorMessageId': 'color_medium_blue'},
  {'value': 0x40e0d0, 'colorMessageId': 'color_turquoise'},
  {'value': 0xced1, 'colorMessageId': 'color_dark_turquoise'},
  {'value': 0xfafad2, 'colorMessageId': 'color_light_golden_rod_yellow'},
  {'value': 0x9400d3, 'colorMessageId': 'color_dark_violet'},
  {'value': 0x7fffd4, 'colorMessageId': 'color_aquamarine'},
  {'value': 0xffefd5, 'colorMessageId': 'color_papaya_whip'},
  {'value': 0xda70d6, 'colorMessageId': 'color_orchid'},
  {'value': 0xfaebd7, 'colorMessageId': 'color_antique_white'},
  {'value': 0xd8bfd8, 'colorMessageId': 'color_thistle'},
  {'value': 0x9370db, 'colorMessageId': 'color_medium_purple'},
  {'value': 0xdcdcdc, 'colorMessageId': 'color_gainsboro'},
  {'value': 0xdda0dd, 'colorMessageId': 'color_plum'},
  {'value': 0xb0c4de, 'colorMessageId': 'color_light_steel_blue'},
  {'value': 0x8b0000, 'colorMessageId': 'color_dark_red'},
  {'value': 0xfff5ee, 'colorMessageId': 'color_sea_shell'},
  {'value': 0x4169e1, 'colorMessageId': 'color_royal_blue'},
  {'value': 0x8a2be2, 'colorMessageId': 'color_blue_violet'},
  {'value': 0x7cfc00, 'colorMessageId': 'color_lawn_green'},
  {'value': 0xe0ffff, 'colorMessageId': 'color_light_cyan'},
  {'value': 0xb22222, 'colorMessageId': 'color_fire_brick'},
  {'value': 0x87ceeb, 'colorMessageId': 'color_sky_blue'},
  {'value': 0x6495ed, 'colorMessageId': 'color_cornflower_blue'},
  {'value': 0x7b68ee, 'colorMessageId': 'color_medium_slate_blue'},
  {'value': 0xff, 'colorMessageId': 'color_blue'},
  {'value': 0xf0fff0, 'colorMessageId': 'color_honeydew'},
  {'value': 0xba55d3, 'colorMessageId': 'color_medium_orchid'},
  {'value': 0xf5f5f5, 'colorMessageId': 'color_white_smoke'},
  {'value': 0xffff00, 'colorMessageId': 'color_yellow'},
  {'value': 0x87cefa, 'colorMessageId': 'color_light_sky_blue'},
  {'value': 0xbfff, 'colorMessageId': 'color_deep_sky_blue'},
  {'value': 0xfffafa, 'colorMessageId': 'color_snow'},
  {'value': 0x66cdaa, 'colorMessageId': 'color_medium_aqua_marine'},
  {'value': 0x7fff00, 'colorMessageId': 'color_chartreuse'},
  {'value': 0x1e90ff, 'colorMessageId': 'color_dodger_blue'},
];

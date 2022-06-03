// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_next_e2e_test_base.js']);

/**
 * Test fixture for Color.
 */
ChromeVoxColorTest = class extends ChromeVoxNextE2ETest {};


SYNC_TEST_F('ChromeVoxColorTest', 'FindDistanceTest', function() {
  // Hexadecimal representations of colors.
  const red = 0xff0000;
  const lime = 0x00ff00;
  const blue = 0x0000ff;
  const opaqueRed = 0xffff0000;
  const transparentLime = 0x0000ff00;

  assertEquals(Color.findDistance(red, lime), Color.findDistance(lime, blue));
  // Opacity should not factor into this calculation.
  assertEquals(
      Color.findDistance(red, lime),
      Color.findDistance(opaqueRed, transparentLime));
});

SYNC_TEST_F('ChromeVoxColorTest', 'FindClosestMatchingColorTest', function() {
  const white = 0xffffff;
  const red = 0xff0000;
  const lime = 0x00ff00;
  const blue = 0x0000ff;
  const black = 0x000000;

  const gmailDefaultTextColor = 0x222222;
  const looksLikePink = 0xF4CCCC;
  const looksLikeGreen = 0x38761D;
  const looksLikeDarkGrey = 0x0C343D;
  const unknownColor = 0x003DAC;

  // Exact matches.
  assertEquals('White', Color.findClosestMatchingColor(white));
  assertEquals('Red', Color.findClosestMatchingColor(red));
  assertEquals('Lime', Color.findClosestMatchingColor(lime));
  assertEquals('Blue', Color.findClosestMatchingColor(blue));
  assertEquals('Black', Color.findClosestMatchingColor(black));

  // Inexact matches.
  assertEquals('Black', Color.findClosestMatchingColor(gmailDefaultTextColor));
  assertEquals('Pink', Color.findClosestMatchingColor(looksLikePink));
  assertEquals('Forest Green', Color.findClosestMatchingColor(looksLikeGreen));
  assertEquals(
      'Dark Slate Grey', Color.findClosestMatchingColor(looksLikeDarkGrey));

  // No match.
  assertEquals('', Color.findClosestMatchingColor(unknownColor));
});

SYNC_TEST_F('ChromeVoxColorTest', 'GetOpacityPercentageTest', function() {
  const opaqueRed = 0xffff0000;
  const transparentLime = 0x0000ff00;
  const translucentBlue = 0x800000ff;

  assertEquals(100, Color.getOpacityPercentage(opaqueRed));
  assertEquals(0, Color.getOpacityPercentage(transparentLime));
  assertEquals(50, Color.getOpacityPercentage(translucentBlue));
});

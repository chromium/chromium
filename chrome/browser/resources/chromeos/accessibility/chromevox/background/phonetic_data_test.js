// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test fixture for PhoneticData.
 */
ChromeVoxPhoneticDataTest = class extends testing.Test {};

/** @override */
ChromeVoxPhoneticDataTest.prototype.extraLibraries = [
  '../../common/testing/assert_additions.js',
  '../testing/fake_dom.js',
  'phonetic_data.js',
  '../third_party/tamachiyomi/ja_phonetic_data.js',
];

TEST_F('ChromeVoxPhoneticDataTest', 'JaPhoneticReading', function() {
  assertEquals('ひらがな あいうえお', PhoneticData.forText('あいうえお', 'ja'));
  assertEquals('カタカナ アイウエオ', PhoneticData.forText('アイウエオ', 'ja'));
  assertEquals(
      'ひらがな あいうえお カタカナ アイウエオ',
      PhoneticData.forText('あいうえおアイウエオ', 'ja'));
  assertEquals('ひらがな か', PhoneticData.forText('か', 'ja'));
  assertEquals('ひらがな か', PhoneticData.forCharacter('か', 'ja'));
  assertEquals(
      'テンキヨホウ ノ テン クウキ ノ キ', PhoneticData.forText('天気', 'ja'));
});

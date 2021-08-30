// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test fixture for PhoneticData.
 */
ChromeVoxPhoneticDataTest = class extends testing.Test {
  /** @override */
  setUp() {
    JaPhoneticData.init(JA_TEST_MAP);
  }
};

/** @override */
ChromeVoxPhoneticDataTest.prototype.extraLibraries = [
  '../../common/testing/assert_additions.js',
  '../testing/fake_dom.js',
  'phonetic_data.js',
  '../third_party/tamachiyomi/ja_phonetic_data.js',
];

/**
 * This is only for test. Note that reading is different from production.
 * @type {Object<string, string>}
 */
JA_TEST_MAP = {
  'A': 'エイ アニマル',
  'a': 'エイ アニマル',
  '1': 'イチ',
  'Ａ': 'エイ アニマル',
  'ａ': 'エイ アニマル',
  '１': 'イチ',
  '亜': 'アジア ノ ア',
  'ー': 'チョウオン'
};

// TODO(crbug/1195393): Polish phonetic readings so that users can disambiguate
// more precisely.
TEST_F('ChromeVoxPhoneticDataTest', 'forCharacterJa', function() {
  assertEquals('ひらがな あ', PhoneticData.forCharacter('あ', 'ja'));
  assertEquals('カタカナ ア', PhoneticData.forCharacter('ア', 'ja'));
  assertEquals('ひらがな っ', PhoneticData.forCharacter('っ', 'ja'));
  assertEquals('カタカナ ッ', PhoneticData.forCharacter('ッ', 'ja'));
  assertEquals('ハンカク ｱ', PhoneticData.forCharacter('ｱ', 'ja'));
  assertEquals('ハンカク ｯ', PhoneticData.forCharacter('ｯ', 'ja'));
  assertEquals('エイ アニマル', PhoneticData.forCharacter('A', 'ja'));
  assertEquals('エイ アニマル', PhoneticData.forCharacter('a', 'ja'));
  assertEquals('イチ', PhoneticData.forCharacter('1', 'ja'));
  assertEquals('エイ アニマル', PhoneticData.forCharacter('Ａ', 'ja'));
  assertEquals('エイ アニマル', PhoneticData.forCharacter('ａ', 'ja'));
  assertEquals('イチ', PhoneticData.forCharacter('１', 'ja'));
  assertEquals('アジア ノ ア', PhoneticData.forCharacter('亜', 'ja'));
});

TEST_F('ChromeVoxPhoneticDataTest', 'forTextJaSingleCharacter', function() {
  assertEquals('ひらがな あ', PhoneticData.forText('あ', 'ja'));
  assertEquals('カタカナ ア', PhoneticData.forText('ア', 'ja'));
  assertEquals('ひらがな っ', PhoneticData.forText('っ', 'ja'));
  assertEquals('カタカナ ッ', PhoneticData.forText('ッ', 'ja'));
  assertEquals('ハンカク ｱ', PhoneticData.forText('ｱ', 'ja'));
  assertEquals('ハンカク ｯ', PhoneticData.forText('ｯ', 'ja'));
  assertEquals('エイ アニマル', PhoneticData.forText('A', 'ja'));
  assertEquals('エイ アニマル', PhoneticData.forText('a', 'ja'));
  assertEquals('イチ', PhoneticData.forText('1', 'ja'));
  assertEquals('エイ アニマル', PhoneticData.forText('Ａ', 'ja'));
  assertEquals('エイ アニマル', PhoneticData.forText('ａ', 'ja'));
  assertEquals('イチ', PhoneticData.forText('１', 'ja'));
  assertEquals('アジア ノ ア', PhoneticData.forText('亜', 'ja'));
});

TEST_F('ChromeVoxPhoneticDataTest', 'forTextJaPairCharacters', function() {
  assertEquals('ひらがな ああ', PhoneticData.forText('ああ', 'ja'));
  assertEquals('ひらがな あ カタカナ ア', PhoneticData.forText('あア', 'ja'));
  assertEquals('ひらがな あ ハンカク ｱ', PhoneticData.forText('あｱ', 'ja'));
  assertEquals('ひらがな あ エイ アニマル', PhoneticData.forText('あA', 'ja'));
  assertEquals('ひらがな あ アジア ノ ア', PhoneticData.forText('あ亜', 'ja'));
  assertEquals('カタカナ アア', PhoneticData.forText('アア', 'ja'));
  assertEquals('ハンカク ｱｱ', PhoneticData.forText('ｱｱ', 'ja'));
  assertEquals('エイ アニマル エイ アニマル', PhoneticData.forText('AA', 'ja'));
  assertEquals('アジア ノ ア アジア ノ ア', PhoneticData.forText('亜亜', 'ja'));
});

TEST_F('ChromeVoxPhoneticDataTest', 'forTextJaLongSound', function() {
  assertEquals('ひらがな あ チョウオン', PhoneticData.forText('あー', 'ja'));
  assertEquals('カタカナ ア チョウオン', PhoneticData.forText('アー', 'ja'));
  assertEquals('ハンカク ｱｰ', PhoneticData.forText('ｱｰ', 'ja'));
  assertEquals('チョウオン チョウオン', PhoneticData.forText('ーー', 'ja'));
  assertEquals('アジア ノ ア チョウオン', PhoneticData.forText('亜ー', 'ja'));
});
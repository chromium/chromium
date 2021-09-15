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
 * This map is not always used for determining phonetic readings. For example,
 * sometimes only a character type is prepended to a character from the
 * alphabet.
 * @type {Object<string, string>}
 * @const
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
  assertEquals('ヒラガナ あ', PhoneticData.forCharacter('あ', 'ja'));
  assertEquals('カタカナ ア', PhoneticData.forCharacter('ア', 'ja'));
  assertEquals('ヒラガナ チイサイ あ', PhoneticData.forCharacter('ぁ', 'ja'));
  assertEquals('カタカナ チイサイ ア', PhoneticData.forCharacter('ァ', 'ja'));
  assertEquals('ハンカク ｱ', PhoneticData.forCharacter('ｱ', 'ja'));
  assertEquals('ハンカク チイサイ ｱ', PhoneticData.forCharacter('ｧ', 'ja'));
  assertEquals('オオモジ A', PhoneticData.forCharacter('A', 'ja'));
  assertEquals('ハンカク a', PhoneticData.forCharacter('a', 'ja'));
  assertEquals('イチ', PhoneticData.forCharacter('1', 'ja'));
  assertEquals('ゼンカクオオモジ Ａ', PhoneticData.forCharacter('Ａ', 'ja'));
  assertEquals('ゼンカク ａ', PhoneticData.forCharacter('ａ', 'ja'));
  assertEquals('イチ', PhoneticData.forCharacter('１', 'ja'));
  assertEquals('アジア ノ ア', PhoneticData.forCharacter('亜', 'ja'));
});

TEST_F('ChromeVoxPhoneticDataTest', 'forTextJaSingleCharacter', function() {
  assertEquals('ヒラガナ あ', PhoneticData.forText('あ', 'ja'));
  assertEquals('カタカナ ア', PhoneticData.forText('ア', 'ja'));
  assertEquals('ヒラガナ チイサイ あ', PhoneticData.forText('ぁ', 'ja'));
  assertEquals('カタカナ チイサイ ア', PhoneticData.forText('ァ', 'ja'));
  assertEquals('ハンカク ｱ', PhoneticData.forText('ｱ', 'ja'));
  assertEquals('ハンカク チイサイ ｱ', PhoneticData.forText('ｧ', 'ja'));
  assertEquals('オオモジ A', PhoneticData.forText('A', 'ja'));
  assertEquals('ハンカク a', PhoneticData.forText('a', 'ja'));
  assertEquals('イチ', PhoneticData.forText('1', 'ja'));
  assertEquals('ゼンカクオオモジ Ａ', PhoneticData.forText('Ａ', 'ja'));
  assertEquals('ゼンカク ａ', PhoneticData.forText('ａ', 'ja'));
  assertEquals('イチ', PhoneticData.forText('１', 'ja'));
  assertEquals('アジア ノ ア', PhoneticData.forText('亜', 'ja'));
});

TEST_F(
    'ChromeVoxPhoneticDataTest', 'forTextJaPairCharacters_StartWithHiragana',
    function() {
      assertEquals('ヒラガナ ああ', PhoneticData.forText('ああ', 'ja'));
      assertEquals(
          'ヒラガナ あ カタカナ ア', PhoneticData.forText('あア', 'ja'));
      assertEquals(
          'ヒラガナ あ ヒラガナ チイサイ あ',
          PhoneticData.forText('あぁ', 'ja'));
      assertEquals(
          'ヒラガナ あ カタカナ チイサイ ア',
          PhoneticData.forText('あァ', 'ja'));
      assertEquals('ヒラガナ あ ハンカク ｱ', PhoneticData.forText('あｱ', 'ja'));
      assertEquals(
          'ヒラガナ あ ハンカク チイサイ ｱ', PhoneticData.forText('あｧ', 'ja'));
      assertEquals('ヒラガナ あ オオモジ A', PhoneticData.forText('あA', 'ja'));
      assertEquals('ヒラガナ あ ハンカク a', PhoneticData.forText('あa', 'ja'));
      assertEquals('ヒラガナ あ イチ', PhoneticData.forText('あ1', 'ja'));
      assertEquals(
          'ヒラガナ あ ゼンカクオオモジ Ａ',
          PhoneticData.forText('あＡ', 'ja'));
      assertEquals(
          'ヒラガナ あ ゼンカク ａ', PhoneticData.forText('あａ', 'ja'));
      assertEquals('ヒラガナ あ イチ', PhoneticData.forText('あ１', 'ja'));
      assertEquals(
          'ヒラガナ あ アジア ノ ア', PhoneticData.forText('あ亜', 'ja'));
    });

TEST_F(
    'ChromeVoxPhoneticDataTest',
    'forTextJaPairCharacters_StartWithHiraganaSmallLetter', function() {
      assertEquals(
          'ヒラガナ チイサイ あ ヒラガナ あ',
          PhoneticData.forText('ぁあ', 'ja'));
      assertEquals(
          'ヒラガナ チイサイ あぁ', PhoneticData.forText('ぁぁ', 'ja'));
    });

TEST_F('ChromeVoxPhoneticDataTest', 'forTextJaLongSound', function() {
  assertEquals('ヒラガナ あ チョウオン', PhoneticData.forText('あー', 'ja'));
  assertEquals('カタカナ ア チョウオン', PhoneticData.forText('アー', 'ja'));
  assertEquals('ハンカク ｱｰ', PhoneticData.forText('ｱｰ', 'ja'));
  assertEquals('チョウオン チョウオン', PhoneticData.forText('ーー', 'ja'));
  assertEquals('アジア ノ ア チョウオン', PhoneticData.forText('亜ー', 'ja'));
});
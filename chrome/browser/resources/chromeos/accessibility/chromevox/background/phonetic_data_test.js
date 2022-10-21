// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../common/testing/accessibility_test_base.js']);

/**
 * Test fixture for PhoneticData.
 */
ChromeVoxPhoneticDataTest = class extends AccessibilityTestBase {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    // Alphabetical based on file path.
    await importModule(
        'PhoneticData', '/chromevox/background/phonetic_data.js');

    JaPhoneticData.init(JA_TEST_MAP);
  }
};

/** @override */
ChromeVoxPhoneticDataTest.prototype.extraLibraries = [
  '../../common/testing/assert_additions.js',
  '../testing/fake_dom.js',
  '../third_party/tamachiyomi/ja_phonetic_data.js',
];

/**
 * This is only for test. Note that reading is different from production.
 * This map is not always used for determining phonetic readings. For example,
 * sometimes only a character type is prepended to a character from the
 * alphabet.
 * @type {Map<string, string>}
 * @const
 */
JA_TEST_MAP = new Map([
  ['あ', 'アサヒ ノ ア'],
  ['ア', 'アサヒ ノ ア'],
  ['ｱ', 'アサヒ ノ ア'],
  ['A', 'エイ アニマル'],
  ['a', 'エイ アニマル'],
  ['1', 'イチ'],
  ['Ａ', 'エイ アニマル'],
  ['ａ', 'エイ アニマル'],
  ['１', 'イチ'],
  ['ー', 'チョウオン'],
  ['。', 'マル'],
  ['@', 'アットマーク'],
  ['＠', 'アットマーク'],
  ['Α', 'ギリシャ アルファ'],
  ['α', 'ギリシャ アルファ'],
  ['亜', 'アジア ノ ア'],
  ['今', 'コンゲツノコン'],
  ['日', 'ニチヨウビノニチ'],
  ['天', 'テンキヨホウノテン'],
  ['気', 'クウキノキ'],
  ['働', 'ロウドウノドウ'],
]);

// TODO(crbug/1195393): Polish phonetic readings so that users can disambiguate
// more precisely.
TEST_F('ChromeVoxPhoneticDataTest', 'forCharacterJa', function() {
  assertEquals('ヒラガナ アサヒ ノ ア', PhoneticData.forCharacter('あ', 'ja'));
  assertEquals('カタカナ アサヒ ノ ア', PhoneticData.forCharacter('ア', 'ja'));
  assertEquals(
      'ヒラガナ チイサイ アサヒ ノ ア', PhoneticData.forCharacter('ぁ', 'ja'));
  assertEquals(
      'カタカナ チイサイ アサヒ ノ ア', PhoneticData.forCharacter('ァ', 'ja'));
  assertEquals('ハンカク アサヒ ノ ア', PhoneticData.forCharacter('ｱ', 'ja'));
  assertEquals(
      'ハンカク チイサイ アサヒ ノ ア', PhoneticData.forCharacter('ｧ', 'ja'));

  // If the capitalStrategy is announceCapitals, 'A' is read as '大文字のA エイ
  // アニマル'
  assertEquals('エイ アニマル', PhoneticData.forCharacter('A', 'ja'));

  assertEquals('エイ アニマル', PhoneticData.forCharacter('a', 'ja'));
  assertEquals('ハンカク イチ', PhoneticData.forCharacter('1', 'ja'));
  assertEquals('ハンカク アットマーク', PhoneticData.forCharacter('@', 'ja'));
  assertEquals('ゼンカク エイ アニマル', PhoneticData.forCharacter('Ａ', 'ja'));
  assertEquals('ゼンカク エイ アニマル', PhoneticData.forCharacter('ａ', 'ja'));
  assertEquals('ゼンカク イチ', PhoneticData.forCharacter('１', 'ja'));
  assertEquals('ゼンカク アットマーク', PhoneticData.forCharacter('＠', 'ja'));
  assertEquals('ギリシャ アルファ', PhoneticData.forCharacter('Α', 'ja'));
  assertEquals('ギリシャ アルファ', PhoneticData.forCharacter('α', 'ja'));
  assertEquals('アジア ノ ア', PhoneticData.forCharacter('亜', 'ja'));
});

TEST_F('ChromeVoxPhoneticDataTest', 'forTextJaSingleCharacter', function() {
  assertEquals('あ', PhoneticData.forText('あ', 'ja'));
  assertEquals('カタカナ ア', PhoneticData.forText('ア', 'ja'));
  assertEquals('あ', PhoneticData.forText('ぁ', 'ja'));
  assertEquals('カタカナ チイサイ ア', PhoneticData.forText('ァ', 'ja'));
  assertEquals('ハンカク ｱ', PhoneticData.forText('ｱ', 'ja'));
  assertEquals('ハンカク チイサイ ｱ', PhoneticData.forText('ｧ', 'ja'));
  assertEquals('オオモジ A', PhoneticData.forText('A', 'ja'));
  assertEquals('a', PhoneticData.forText('a', 'ja'));
  assertEquals('1', PhoneticData.forText('1', 'ja'));
  assertEquals('アットマーク', PhoneticData.forText('@', 'ja'));
  assertEquals('ゼンカクオオモジ Ａ', PhoneticData.forText('Ａ', 'ja'));
  assertEquals('ゼンカク ａ', PhoneticData.forText('ａ', 'ja'));
  assertEquals('ゼンカク １', PhoneticData.forText('１', 'ja'));
  assertEquals('ゼンカク アットマーク', PhoneticData.forText('＠', 'ja'));
  assertEquals('オオモジ ギリシャ アルファ', PhoneticData.forText('Α', 'ja'));
  assertEquals('ギリシャ アルファ', PhoneticData.forText('α', 'ja'));
  assertEquals('アジア ノ ア', PhoneticData.forText('亜', 'ja'));
});

TEST_F(
    'ChromeVoxPhoneticDataTest', 'forTextJaPairCharacters_EndWithHiragana',
    function() {
      assertEquals('ああ', PhoneticData.forText('ああ', 'ja'));
      assertEquals('ああ', PhoneticData.forText('ぁあ', 'ja'));
      assertEquals('オオモジ A あ', PhoneticData.forText('Aあ', 'ja'));
    });

TEST_F(
    'ChromeVoxPhoneticDataTest', 'forTextJaPairCharacters_EndWithKatakana',
    function() {
      assertEquals('カタカナ アア', PhoneticData.forText('アア', 'ja'));
      assertEquals(
          'カタカナ チイサイ アア', PhoneticData.forText('ァア', 'ja'));
      assertEquals('あ カタカナ ア', PhoneticData.forText('あア', 'ja'));
    });

TEST_F(
    'ChromeVoxPhoneticDataTest',
    'forTextJaPairCharacters_EndWithHiraganaSmallLetter', function() {
      assertEquals('あぁ', PhoneticData.forText('ぁぁ', 'ja'));
      assertEquals('あぁ', PhoneticData.forText('あぁ', 'ja'));
      assertEquals('アジア ノ ア あ', PhoneticData.forText('亜ぁ', 'ja'));
    });

TEST_F(
    'ChromeVoxPhoneticDataTest',
    'forTextJaPairCharacters_EndWithKatakanaSmallLetter', function() {
      assertEquals(
          'カタカナ チイサイ アァ', PhoneticData.forText('ァァ', 'ja'));
      assertEquals('カタカナ アァ', PhoneticData.forText('アァ', 'ja'));
      assertEquals(
          'あ カタカナ チイサイ ア', PhoneticData.forText('あァ', 'ja'));
    });

TEST_F(
    'ChromeVoxPhoneticDataTest',
    'forTextJaPairCharacters_EndWithHalfWidthKatakana', function() {
      assertEquals('ハンカク ｱｱ', PhoneticData.forText('ｱｱ', 'ja'));
      assertEquals('ハンカク チイサイ ｱｱ', PhoneticData.forText('ｧｱ', 'ja'));
      assertEquals('あ ハンカク ｱ', PhoneticData.forText('あｱ', 'ja'));
    });

TEST_F(
    'ChromeVoxPhoneticDataTest',
    'forTextJaPairCharacters_EndWithHalfWidthKatakanaSmallLetter', function() {
      assertEquals('ハンカク チイサイ ｱｧ', PhoneticData.forText('ｧｧ', 'ja'));
      assertEquals('ハンカク ｱｧ', PhoneticData.forText('ｱｧ', 'ja'));
      assertEquals('あ ハンカク チイサイ ｱ', PhoneticData.forText('あｧ', 'ja'));
    });

TEST_F(
    'ChromeVoxPhoneticDataTest',
    'forTextJaPairCharacters_EndWithHalfWidthAlphabetUpper', function() {
      assertEquals('オオモジ AA', PhoneticData.forText('AA', 'ja'));
      assertEquals(
          'ゼンカクオオモジ Ａ ハンカクオオモジ A',
          PhoneticData.forText('ＡA', 'ja'));
      assertEquals(
          'ゼンカク ａ ハンカクオオモジ A', PhoneticData.forText('ａA', 'ja'));
      assertEquals('あ オオモジ A', PhoneticData.forText('あA', 'ja'));
    });

TEST_F(
    'ChromeVoxPhoneticDataTest',
    'forTextJaPairCharacters_EndWithHalfWidthAlphabetLower', function() {
      assertEquals('aa', PhoneticData.forText('aa', 'ja'));
      assertEquals('あ a', PhoneticData.forText('あa', 'ja'));
      assertEquals(
          'ゼンカクオオモジ Ａ ハンカク a', PhoneticData.forText('Ａa', 'ja'));
    });

TEST_F(
    'ChromeVoxPhoneticDataTest',
    'forTextJaPairCharacters_EndWithHalfWidthNumeric', function() {
      assertEquals('11', PhoneticData.forText('11', 'ja'));
      assertEquals('あ 1', PhoneticData.forText('あ1', 'ja'));
      assertEquals(
          'ゼンカクオオモジ Ａ ハンカク 1', PhoneticData.forText('Ａ1', 'ja'));
    });

TEST_F(
    'ChromeVoxPhoneticDataTest',
    'forTextJaPairCharacters_EndWithHalfWidthSymbol', function() {
      assertEquals(
          'アットマーク アットマーク', PhoneticData.forText('@@', 'ja'));
      assertEquals('あ アットマーク', PhoneticData.forText('あ@', 'ja'));
      assertEquals(
          'ゼンカクオオモジ Ａ ハンカク アットマーク',
          PhoneticData.forText('Ａ@', 'ja'));
    });

TEST_F(
    'ChromeVoxPhoneticDataTest',
    'forTextJaPairCharacters_EndWithFullWidthAlphabetUpper', function() {
      assertEquals('ゼンカクオオモジ ＡＡ', PhoneticData.forText('ＡＡ', 'ja'));
      assertEquals(
          'ゼンカク ａ オオモジ Ａ', PhoneticData.forText('ａＡ', 'ja'));
      assertEquals(
          'あ ゼンカクオオモジ Ａ', PhoneticData.forText('あＡ', 'ja'));
    });

TEST_F(
    'ChromeVoxPhoneticDataTest',
    'forTextJaPairCharacters_EndWithFullWidthAlphabetLower', function() {
      assertEquals('ゼンカク ａａ', PhoneticData.forText('ａａ', 'ja'));
      assertEquals(
          'ゼンカクオオモジ Ａ ａ', PhoneticData.forText('Ａａ', 'ja'));
      assertEquals('あ ゼンカク ａ', PhoneticData.forText('あａ', 'ja'));
    });

TEST_F(
    'ChromeVoxPhoneticDataTest',
    'forTextJaPairCharacters_EndWithFullWidthNumeric', function() {
      assertEquals('ゼンカク １１', PhoneticData.forText('１１', 'ja'));
      assertEquals(
          'ゼンカクオオモジ Ａ １', PhoneticData.forText('Ａ１', 'ja'));
      assertEquals('あ ゼンカク １', PhoneticData.forText('あ１', 'ja'));
    });

TEST_F(
    'ChromeVoxPhoneticDataTest',
    'forTextJaPairCharacters_EndWithFullWidthSymbol', function() {
      assertEquals(
          'ゼンカク アットマーク アットマーク',
          PhoneticData.forText('＠＠', 'ja'));
      assertEquals(
          'ゼンカクオオモジ Ａ アットマーク',
          PhoneticData.forText('Ａ＠', 'ja'));
      assertEquals(
          'あ ゼンカク アットマーク', PhoneticData.forText('あ＠', 'ja'));
    });

TEST_F(
    'ChromeVoxPhoneticDataTest',
    'forTextJaPairCharacters_EndWithFullWidthGreekUpper', function() {
      assertEquals(
          'オオモジ ギリシャ アルファ ギリシャ アルファ',
          PhoneticData.forText('ΑΑ', 'ja'));
      assertEquals(
          'あ オオモジ ギリシャ アルファ', PhoneticData.forText('あΑ', 'ja'));
    });

TEST_F(
    'ChromeVoxPhoneticDataTest',
    'forTextJaPairCharacters_EndWithFullWidthGreekLower', function() {
      assertEquals(
          'ギリシャ アルファ ギリシャ アルファ',
          PhoneticData.forText('αα', 'ja'));
      assertEquals(
          'オオモジ ギリシャ アルファ コモジ ギリシャ アルファ',
          PhoneticData.forText('Αα', 'ja'));
      assertEquals('あ ギリシャ アルファ', PhoneticData.forText('あα', 'ja'));
    });

TEST_F(
    'ChromeVoxPhoneticDataTest', 'forTextJaPairCharacters_EndWithOther',
    function() {
      assertEquals(
          'アジア ノ ア アジア ノ ア', PhoneticData.forText('亜亜', 'ja'));
    });

TEST_F('ChromeVoxPhoneticDataTest', 'forTextJaLongSound', function() {
  assertEquals('あー', PhoneticData.forText('あー', 'ja'));
  assertEquals('カタカナ アー', PhoneticData.forText('アー', 'ja'));
  assertEquals('あー', PhoneticData.forText('ぁー', 'ja'));
  assertEquals('カタカナ チイサイ アー', PhoneticData.forText('ァー', 'ja'));
  assertEquals('ハンカク ｱｰ', PhoneticData.forText('ｱｰ', 'ja'));
  assertEquals('あーー', PhoneticData.forText('あーー', 'ja'));
  assertEquals('チョウオン チョウオン', PhoneticData.forText('ーー', 'ja'));
  assertEquals('アジア ノ ア チョウオン', PhoneticData.forText('亜ー', 'ja'));
});

TEST_F('ChromeVoxPhoneticDataTest', 'forTextSampleSentences', function() {
  assertEquals(
      'コンゲツノコン ニチヨウビノニチ は テンキヨホウノテン クウキノキ です マル',
      PhoneticData.forText('今日は天気です。', 'ja'));
  assertEquals(
      'きょうはてんきです マル',
      PhoneticData.forText('きょうはてんきです。', 'ja'));
  assertEquals(
      'オオモジ G oogle で ロウドウノドウ いています マル',
      PhoneticData.forText('Googleで働いています。', 'ja'));
  assertEquals('オオモジ GOOGLE', PhoneticData.forText('GOOGLE', 'ja'));
  assertEquals(
      'オオモジ GO o オオモジ GLE', PhoneticData.forText('GOoGLE', 'ja'));
  assertEquals('カタカナ キャット', PhoneticData.forText('キャット', 'ja'));
});

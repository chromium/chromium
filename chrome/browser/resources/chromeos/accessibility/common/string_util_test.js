// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['testing/common_e2e_test_base.js']);

/** Test fixture for string_util.js. */
AccessibilityExtensionStringUtilTest = class extends CommonE2ETestBase {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    const imports = TestImportManager.getImports();
    globalThis.StringUtil = imports.StringUtil;
  }
};

AX_TEST_F('AccessibilityExtensionStringUtilTest', 'CommonPrefix', function() {
  assertEquals(
      11, StringUtil.longestCommonPrefixLength('Hello World', 'Hello World'));
  assertEquals(
      0, StringUtil.longestCommonPrefixLength('Hello World', 'Good Morning'));
  assertEquals(
      1, StringUtil.longestCommonPrefixLength('Guten Morgen', 'Good Morning'));

  assertEquals(5, StringUtil.longestCommonPrefixLength('Hello', 'Hello World'));
  assertEquals(5, StringUtil.longestCommonPrefixLength('Hello World', 'Hello'));
  assertEquals(0, StringUtil.longestCommonPrefixLength('Hello', ''));
  assertEquals(0, StringUtil.longestCommonPrefixLength('', 'Hello'));
});

AX_TEST_F('AccessibilityExtensionStringUtilTest', 'CommonSuffix', function() {
  assertEquals(
      11, StringUtil.longestCommonSuffixLength('Hello World', 'Hello World'));
  assertEquals(
      0, StringUtil.longestCommonSuffixLength('Hello World', 'Good Morning'));
  assertEquals(
      4, StringUtil.longestCommonSuffixLength('Good Morning', 'Good Evening'));

  assertEquals(5, StringUtil.longestCommonSuffixLength('World', 'Hello World'));
  assertEquals(5, StringUtil.longestCommonSuffixLength('Hello World', 'World'));
  assertEquals(0, StringUtil.longestCommonSuffixLength('Hello', ''));
  assertEquals(0, StringUtil.longestCommonSuffixLength('', 'Hello'));
});

AX_TEST_F(
    'AccessibilityExtensionStringUtilTest', 'Utf8OffsetTablesEmpty',
    function() {
      const r = StringUtil.buildUtf8OffsetTables('');
      assertEqualsJSON([0], r.utf16ToByte);
      assertEqualsJSON([0], r.byteToUtf16);
      assertEquals(0, r.totalBytes);
    });

AX_TEST_F(
    'AccessibilityExtensionStringUtilTest', 'Utf8OffsetTablesAscii',
    function() {
      const r = StringUtil.buildUtf8OffsetTables('ab');
      assertEqualsJSON([0, 1, 2], r.utf16ToByte);
      assertEqualsJSON([0, 1, 2], r.byteToUtf16);
      assertEquals(2, r.totalBytes);
    });

// U+0100 (Ā) encodes as 2 UTF-8 bytes
AX_TEST_F(
    'AccessibilityExtensionStringUtilTest', 'Utf8OffsetTablesTwoByteUtf8',
    function() {
      const r = StringUtil.buildUtf8OffsetTables('Ā');
      assertEqualsJSON([0, 2], r.utf16ToByte);
      assertEqualsJSON([0, 0, 1], r.byteToUtf16);
      assertEquals(2, r.totalBytes);
    });

// U+3042 (あ) encodes as 3 UTF-8 bytes
AX_TEST_F(
    'AccessibilityExtensionStringUtilTest', 'Utf8OffsetTablesThreeByteUtf8',
    function() {
      const r = StringUtil.buildUtf8OffsetTables('あ');
      assertEqualsJSON([0, 3], r.utf16ToByte);
      assertEqualsJSON([0, 0, 0, 1], r.byteToUtf16);
      assertEquals(3, r.totalBytes);
    });

// entry at totalBytes.
AX_TEST_F(
    'AccessibilityExtensionStringUtilTest', 'Utf8OffsetTablesFourByteUtf8',
    function() {
      const emoji = '😀';
      assertEquals(2, emoji.length);
      const r = StringUtil.buildUtf8OffsetTables(emoji);
      assertEqualsJSON([0, 4, 4], r.utf16ToByte);
      assertEqualsJSON([0, 0, 0, 0, 2], r.byteToUtf16);
      assertEquals(4, r.totalBytes);
    });

AX_TEST_F(
    'AccessibilityExtensionStringUtilTest', 'Utf8OffsetTablesAsciiAndThreeByte',
    function() {
      // 'a' (1 byte) + 'あ' (3 bytes)
      const r = StringUtil.buildUtf8OffsetTables('aあ');
      assertEqualsJSON([0, 1, 4], r.utf16ToByte);
      assertEqualsJSON([0, 1, 1, 1, 2], r.byteToUtf16);
      assertEquals(4, r.totalBytes);
    });

AX_TEST_F(
    'AccessibilityExtensionStringUtilTest', 'Utf8OffsetTablesAsciiAndFourByte',
    function() {
      // 'a' (1 byte) + 😀 (4 bytes, 2 UTF-16 code units)
      const r = StringUtil.buildUtf8OffsetTables('a😀');
      assertEqualsJSON([0, 1, 5, 5], r.utf16ToByte);
      assertEqualsJSON([0, 1, 1, 1, 1, 3], r.byteToUtf16);
      assertEquals(5, r.totalBytes);
    });

AX_TEST_F(
    'AccessibilityExtensionStringUtilTest', 'Utf8OffsetTablesHiragana',
    function() {
      // Two 3-byte chars: あ + い
      const r = StringUtil.buildUtf8OffsetTables('あい');
      assertEqualsJSON([0, 3, 6], r.utf16ToByte);
      assertEqualsJSON([0, 0, 0, 1, 1, 1, 2], r.byteToUtf16);
      assertEquals(6, r.totalBytes);
    });

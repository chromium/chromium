// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../select_to_speak/select_to_speak_e2e_test_base.js']);

/**
 * Test fixture for word_utils.js.
 */
SelectToSpeakWordUtilsUnitTest = class extends SelectToSpeakE2ETest {
  async setUpDeferred() {
    await super.setUpDeferred();
    await importModule('WordUtils', '/common/word_utils.js');
  }
};

AX_TEST_F(
    'SelectToSpeakWordUtilsUnitTest', 'getNextWordStartWithoutWordStarts',
    function() {
      const node = {node: {}};
      assertEquals(0, WordUtils.getNextWordStart('kitty cat', 0, node));
      assertEquals(1, WordUtils.getNextWordStart(' kitty cat', 0, node));
      assertEquals(6, WordUtils.getNextWordStart('kitty cat', 5, node));
      assertEquals(6, WordUtils.getNextWordStart('kitty cat', 6, node));
      assertEquals(7, WordUtils.getNextWordStart('kitty "cat"', 5, node));
    });

AX_TEST_F(
    'SelectToSpeakWordUtilsUnitTest', 'getNextWordEndWithoutWordEnds',
    function() {
      const node = {node: {}};
      assertEquals(5, WordUtils.getNextWordEnd('kitty cat', 0, node));
      assertEquals(6, WordUtils.getNextWordEnd(' kitty cat', 0, node));
      assertEquals(9, WordUtils.getNextWordEnd('kitty cat', 6, node));
      assertEquals(9, WordUtils.getNextWordEnd('kitty cat', 7, node));
    });

AX_TEST_F('SelectToSpeakWordUtilsUnitTest', 'getNextWordStart', function() {
  const inlineText = {wordStarts: [0, 6], name: 'kitty cat'};
  const staticText = {children: [inlineText], name: 'kitty cat'};
  const node = {node: staticText, startChar: 0, hasInlineText: true};
  assertEquals(0, WordUtils.getNextWordStart('kitty cat', 0, node));
  assertEquals(6, WordUtils.getNextWordStart('kitty cat', 5, node));
  assertEquals(6, WordUtils.getNextWordStart('kitty cat', 6, node));

  node.startChar = 10;
  assertEquals(10, WordUtils.getNextWordStart('once upon kitty cat', 9, node));
  assertEquals(16, WordUtils.getNextWordStart('once upon kitty cat', 15, node));

  // Should return the default if the inlineText children are missing.
  staticText.children = [];
  assertEquals(
      10, WordUtils.getNextWordStart('once upon a kitty cat', 10, node));
});

AX_TEST_F(
    'SelectToSpeakWordUtilsUnitTest', 'getNextWordStartIgnoresStartCharOffset',
    function() {
      const inlineText = {
        wordStarts: [0, 5, 10, 16],
        name: 'once upon kitty cat',
      };
      const staticText = {children: [inlineText], name: 'once upon kitty cat'};
      const node = {node: staticText, startChar: 9, hasInlineText: true};

      // If search within the node and ignore starChar offset, returns the index
      // of character "k".
      assertEquals(
          10, WordUtils.getNextWordStart('once upon kitty cat', 9, node, true));
      // If consider the starChar offset, returns the index of character "o"
      // with offset.
      assertEquals(
          9, WordUtils.getNextWordStart('once upon kitty cat', 9, node, false));
      // Should return the default if the inlineText children are missing.
      staticText.children = [];
      assertEquals(
          10, WordUtils.getNextWordStart('once upon a kitty cat', 10, node));
    });

AX_TEST_F(
    'SelectToSpeakWordUtilsUnitTest', 'getNextWordStartMultipleChildren',
    function() {
      const inlineText1 = {
        wordStarts: [0, 6],
        name: 'kitty cat ',
        indexInParent: 0,
      };
      const inlineText2 = {
        wordStarts: [0, 3],
        name: 'is cute',
        indexInParent: 1,
      };
      const staticText = {
        children: [inlineText1, inlineText2],
        name: 'kitty cat is cute',
      };
      inlineText1.parent = staticText;
      inlineText2.parent = staticText;
      const node = {node: staticText, startChar: 0, hasInlineText: true};
      assertEquals(
          10, WordUtils.getNextWordStart('kitty cat is cute', 7, node));
      assertEquals(
          13, WordUtils.getNextWordStart('kitty cat is cute', 11, node));
    });

AX_TEST_F('SelectToSpeakWordUtilsUnitTest', 'getNextWordEnd', function() {
  const inlineText = {wordEnds: [5, 9], name: 'kitty cat'};
  const staticText = {children: [inlineText], name: 'kitty cat'};
  const node = {node: staticText, startChar: 0, hasInlineText: true};
  assertEquals(5, WordUtils.getNextWordEnd('kitty cat', 0, node));
  assertEquals(5, WordUtils.getNextWordEnd('kitty cat', 4, node));
  assertEquals(9, WordUtils.getNextWordEnd('kitty cat', 5, node));
  assertEquals(9, WordUtils.getNextWordEnd('kitty cat', 6, node));

  node.startChar = 10;
  assertEquals(15, WordUtils.getNextWordEnd('once upon kitty cat', 9, node));
  assertEquals(19, WordUtils.getNextWordEnd('once upon kitty cat', 17, node));

  // Should return the default if the inlineText children are missing.
  staticText.children = [];
  assertEquals(5, WordUtils.getNextWordEnd('kitty cat', 4, node));
});

AX_TEST_F(
    'SelectToSpeakWordUtilsUnitTest', 'getNextWordEndMultipleChildren',
    function() {
      const inlineText1 = {
        wordEnds: [5, 9],
        name: 'kitty cat ',
        indexInParent: 0,
      };
      const inlineText2 = {wordEnds: [2, 7], name: 'is cute', indexInParent: 1};
      const staticText = {
        children: [inlineText1, inlineText2],
        name: 'kitty cat is cute',
      };
      inlineText1.parent = staticText;
      inlineText2.parent = staticText;
      const node = {node: staticText, startChar: 0, hasInlineText: true};
      assertEquals(12, WordUtils.getNextWordEnd('kitty cat is cute', 10, node));
      assertEquals(17, WordUtils.getNextWordEnd('kitty cat is cute', 13, node));
    });

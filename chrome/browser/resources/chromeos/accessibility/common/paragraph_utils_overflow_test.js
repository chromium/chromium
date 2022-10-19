// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../select_to_speak/select_to_speak_e2e_test_base.js']);

/**
 * Browser tests for select-to-speak's feature to filter out overflow text.
 */
SelectToSpeakParagraphOverflowTest = class extends SelectToSpeakE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await importModule('ParagraphUtils', '/common/paragraph_utils.js');
  }

  generateHorizentalOverflowText(text) {
    return (
        '<div style="width: 50px; overflow: hidden">' +
        '<p style="width: 200px">$text</p>'.replace('$text', text) + '</div>');
  }

  generateVerticalOverflowText(visibleText, overflowText) {
    return (
        '<div style="height: 30px; overflow: hidden">' +
        '<p>$text</p>'.replace('$text', visibleText) +
        '<p>$text</p>'.replace('$text', overflowText) + '</div>');
  }

  generateEntirelyOverflowText(text) {
    return (
        '<div style="width: 50px; overflow:hidden">' +
        '<p style="width: 1000px">' +
        '<span>long text to keep the second node overflow entirely</span>' +
        '$text</p>'.replace('$text', text) + '</div>');
  }

  generateVisibleText(text) {
    return (
        '<div style="width: 50px">' +
        '<p style="width: 200px">$text</p>'.replace('$text', text) + '</div>');
  }
};

AX_TEST_F(
    'SelectToSpeakParagraphOverflowTest',
    'ReplaceseHorizentalOverflowTextWithSpace', async function() {
      const inputText = 'This text overflows partially';
      const root = await this.runWithLoadedTree(
          this.generateHorizentalOverflowText(inputText));
      const overflowText = root.find({
        role: chrome.automation.RoleType.INLINE_TEXT_BOX,
        attributes: {name: inputText},
      });
      var nodeGroup = ParagraphUtils.buildNodeGroup(
          [overflowText], 0 /* index */, {clipOverflowWords: true});

      // The output text should have the same length of the input text
      // plus a space character at the end.
      assertEquals(nodeGroup.text.length, inputText.length + 1);
      // The output text should have less non-empty characters compared
      // to the input text, as any overflow word will be replaced as
      // space characters.
      assertTrue(
          nodeGroup.text.replace(/ /g, '').length <
          inputText.replace(/ /g, '').length);
    });

AX_TEST_F(
    'SelectToSpeakParagraphOverflowTest',
    'ReplaceseVerticalOverflowTextWithSpace', async function() {
      const visibleText = 'This text is visible';
      const overflowText = 'This text overflows';
      const root = await this.runWithLoadedTree(
          this.generateVerticalOverflowText(visibleText, overflowText));
      // Find the visible text.
      const visibleTextNode = root.find({
        role: chrome.automation.RoleType.INLINE_TEXT_BOX,
        attributes: {name: visibleText},
      });
      var nodeGroup = ParagraphUtils.buildNodeGroup(
          [visibleTextNode], 0 /* index */, {clipOverflowWords: true});
      // The output text should have the same length of the visible text
      // plus a space character at the end.
      assertEquals(nodeGroup.text.length, visibleText.length + 1);
      // The output text should be the same of the input text.
      assertEquals(
          nodeGroup.text.replace(/ /g, ''), visibleText.replace(/ /g, ''));

      // Find the overflow text.
      const overflowTextNode = root.find({
        role: chrome.automation.RoleType.INLINE_TEXT_BOX,
        attributes: {name: overflowText},
      });
      var nodeGroup = ParagraphUtils.buildNodeGroup(
          [overflowTextNode], 0 /* index */, {clipOverflowWords: true});

      // The output text should have the same length of the overflow text
      // plus a space character at the end.
      assertEquals(nodeGroup.text.length, overflowText.length + 1);
      // The output text should only have space characters.
      assertEquals(nodeGroup.text.replace(/ /g, '').length, 0);
    });

AX_TEST_F(
    'SelectToSpeakParagraphOverflowTest',
    'ReplacesEntirelyOverflowTextWithSpace', async function() {
      const inputText = 'This text overflows entirely';
      const root = await this.runWithLoadedTree(
          this.generateEntirelyOverflowText(inputText));
      const overflowText = root.find({
        role: chrome.automation.RoleType.INLINE_TEXT_BOX,
        attributes: {name: inputText},
      });
      var nodeGroup = ParagraphUtils.buildNodeGroup(
          [overflowText], 0 /* index */, {clipOverflowWords: true});

      // The output text should have the same length of the input text
      // plus a space character at the end.
      assertEquals(nodeGroup.text.length, inputText.length + 1);
      // The output text should have zero non-empty character.
      assertEquals(nodeGroup.text.replace(/ /g, '').length, 0);
    });

AX_TEST_F(
    'SelectToSpeakParagraphOverflowTest', 'OutputsVisibleText',
    async function() {
      const inputText = 'This text is visible';
      const root =
          await this.runWithLoadedTree(this.generateVisibleText(inputText));
      const visibleText = root.find({
        role: chrome.automation.RoleType.INLINE_TEXT_BOX,
        attributes: {name: inputText},
      });
      var nodeGroup = ParagraphUtils.buildNodeGroup(
          [visibleText], 0 /* index */, {clipOverflowWords: true});

      // The output text should have the same length of the input text plus a
      // space character at the end.
      assertEquals(nodeGroup.text.length, inputText.length + 1);
      // The output text should have same non-empty words as the input text.
      assertEquals(
          nodeGroup.text.replace(/ /g, ''), inputText.replace(/ /g, ''));
    });

AX_TEST_F(
    'SelectToSpeakParagraphOverflowTest',
    'DoesNotClipOverflowWordsWhenDisabled', async function() {
      const inputText = 'This text overflows entirely';
      const root = await this.runWithLoadedTree(
          this.generateEntirelyOverflowText(inputText));
      const overflowText = root.find({
        role: chrome.automation.RoleType.INLINE_TEXT_BOX,
        attributes: {name: inputText},
      });
      var nodeGroup = ParagraphUtils.buildNodeGroup(
          [overflowText], 0 /* index */, {clipOverflowWords: false});

      // The output text should have the same length of the input text
      // plus a space character at the end.
      assertEquals(nodeGroup.text.length, inputText.length + 1);
      // The output text should have same non-empty words as the input
      // text.
      assertEquals(
          nodeGroup.text.replace(/ /g, ''), inputText.replace(/ /g, ''));
    });

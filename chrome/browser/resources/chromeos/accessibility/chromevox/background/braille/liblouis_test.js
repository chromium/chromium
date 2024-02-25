// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for the liblouis wasm wrapper, as seen from
 *    the JavaScript interface.
 */

// Include test fixture.
GEN_INCLUDE(['../../testing/chromevox_e2e_test_base.js']);

ChromeVoxLibLouisTest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    const path = chrome.extension.getURL(
        'chromevox/third_party/liblouis/liblouis_wrapper.js');
    this.liblouis = await LibLouis.create(path, '');
  }

  async backTranslate(tableNames, buffer) {
    const translator = await this.liblouis.getTranslator(tableNames);
    return new Promise(resolve => translator.backTranslate(buffer, resolve));
  }

  async translate(tableNames, text) {
    const translator = await this.liblouis.getTranslator(tableNames);
    return new Promise(
        resolve => translator.translate(text, [], (...args) => resolve(args)));
  }
};

function assertEqualsUint8Array(expected, actual) {
  const asArray = Array.from(new Uint8Array(actual));
  assertEqualsJSON(expected, asArray);
}

AX_TEST_F(
    'ChromeVoxLibLouisTest', 'TranslateComputerBraille', async function() {
      const [cells, textToBraille, brailleToText] =
          await this.translate('en-us-comp8.ctb', 'Hello!');
      assertEqualsUint8Array([0x53, 0x11, 0x07, 0x07, 0x15, 0x2e], cells);
      assertEqualsJSON([0, 1, 2, 3, 4, 5], textToBraille);
      assertEqualsJSON([0, 1, 2, 3, 4, 5], brailleToText);
    });

AX_TEST_F('ChromeVoxLibLouisTest', 'MAYBE_CheckAllTables', async function() {
  const tables = await new Promise(resolve => BrailleTable.getAll(resolve));
  for (const table of tables) {
    const translator = await this.liblouis.getTranslator(table.fileNames);
    assertNotEquals(
        null, translator,
        'Table ' + JSON.stringify(table) + ' should be valid');
  }
}, `
#if defined(MEMORY_SANITIZER)
#define MAYBE_CheckAllTables DISABLED_CheckAllTables
#else
#define MAYBE_CheckAllTables CheckAllTables
#endif
`);

AX_TEST_F(
    'ChromeVoxLibLouisTest', 'BackTranslateComputerBraille', async function() {
      const cells = new Uint8Array([0x53, 0x11, 0x07, 0x07, 0x15, 0x2e]);
      const text = await this.backTranslate('en-us-comp8.ctb', cells.buffer);
      assertEquals('Hello!', text);
    });

AX_TEST_F(
    'ChromeVoxLibLouisTest', 'TranslateGermanGrade2Braille', async function() {
      // This is one of the moderately large tables.
      const [cells, textToBraille, brailleToText] =
          await this.translate('de-g2.ctb', 'München');
      assertEqualsUint8Array([0x0d, 0x33, 0x1d, 0x39, 0x09], cells);
      assertEqualsJSON([0, 1, 2, 3, 3, 4, 4], textToBraille);
      assertEqualsJSON([0, 1, 2, 3, 5], brailleToText);
    });

AX_TEST_F(
    'ChromeVoxLibLouisTest', 'TranslateSpaceIsNotDropped', async function() {
      const [cells, textToBraille, brailleToText] =
          await this.translate('en-ueb-g2.ctb', ' ');
      assertEqualsUint8Array([0x0], cells);
    });

AX_TEST_F(
    'ChromeVoxLibLouisTest', 'BackTranslateGermanComputerBraille',
    async function() {
      const cells = new Uint8Array([0xb3]);
      const text = await this.backTranslate('de-de-comp8.ctb', cells.buffer);
      assertEquals('ü', text);
    });

AX_TEST_F(
    'ChromeVoxLibLouisTest',
    'BackTranslateUSEnglishGrade2PreservesTrailingSpace', async function() {
      // A full braille cell (dots 1-6) is 'for' when backtranslated.
      const cells = new Uint8Array([0b111111, 0]);
      const text = await this.backTranslate('en-ueb-g2.ctb', cells.buffer);
      assertNotEquals(null, text);
      assertEquals('for ', text);
    });

AX_TEST_F('ChromeVoxLibLouisTest', 'BackTranslateEmptyCells', async function() {
  const text =
      await this.backTranslate('de-de-comp8.ctb', new Uint8Array().buffer);
  assertNotEquals(null, text);
  assertEquals(0, text.length);
});

AX_TEST_F('ChromeVoxLibLouisTest', 'GetInvalidTranslator', async function() {
  console.log('Expecting an error from liblouis');
  const translator = await this.liblouis.getTranslator('nonexistant-table');
  assertEquals(null, translator);
});

AX_TEST_F('ChromeVoxLibLouisTest', 'KeyEventStaticData', async function() {
  const [cells, textToBraille, brailleToText] = await this.translate(
      'en-us-comp8.ctb', 'abcdefghijklmnopqrstuvwxyz 0123456789');
  // A-Z.
  const view = new Uint8Array(cells);
  for (let i = 0; i < 26; i++) {
    assertEquals(
        String.fromCharCode(i + 65),
        BrailleKeyEvent.brailleDotsToStandardKeyCode[view[i]]);
  }

  // 0-9.
  for (let i = 27; i < 37; i++) {
    assertEquals(
        String.fromCharCode(i + 21),
        BrailleKeyEvent.brailleDotsToStandardKeyCode[view[i]]);
  }
});

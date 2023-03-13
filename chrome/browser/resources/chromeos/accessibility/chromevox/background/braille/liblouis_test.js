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
    await importModule(
        'BrailleTable', '/chromevox/common/braille/braille_table.js');
    await importModule('LibLouis', '/chromevox/background/braille/liblouis.js');
    await importModule(
        'BrailleKeyEvent', '/chromevox/common/braille/braille_key_types.js');
  }
};

async function withTranslator(liblouis, tableNames) {
  return new Promise(resolve => liblouis.getTranslator(tableNames, resolve));
}

async function translate(liblouis, tableNames, text) {
  const translator = await withTranslator(liblouis, tableNames);
  return new Promise(
      resolve => translator.translate(text, [], (...args) => resolve(args)));
}

async function backTranslate(liblouis, tableNames, buffer) {
  const translator = await withTranslator(liblouis, tableNames);
  return new Promise(resolve => translator.backTranslate(buffer, resolve));
}

function assertEqualsUint8Array(expected, actual) {
  const asArray = Array.from(new Uint8Array(actual));
  assertEqualsJSON(expected, asArray);
}

function LIBLOUIS_TEST_F(testName, testFunc, opt_preamble) {
  // This needs to stay a function - don't convert to arrow function.
  const wrappedTestFunc = async function() {
    const path = chrome.extension.getURL(
        'chromevox/background/braille/liblouis_wrapper.js');
    const liblouis = await LibLouis.create(path, '');
    testFunc(liblouis);
  };
  AX_TEST_F('ChromeVoxLibLouisTest', testName, wrappedTestFunc, opt_preamble);
}

LIBLOUIS_TEST_F('testTranslateComputerBraille', async function(liblouis) {
  const [cells, textToBraille, brailleToText] =
      await translate(liblouis, 'en-us-comp8.ctb', 'Hello!');
  assertEqualsUint8Array([0x53, 0x11, 0x07, 0x07, 0x15, 0x2e], cells);
  assertEqualsJSON([0, 1, 2, 3, 4, 5], textToBraille);
  assertEqualsJSON([0, 1, 2, 3, 4, 5], brailleToText);
});

LIBLOUIS_TEST_F('MAYBE_CheckAllTables', async function(liblouis) {
  const tables = await new Promise(resolve => BrailleTable.getAll(resolve));
  for (const table of tables) {
    const translator = await this.withTranslator(liblouis, table.fileNames);
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

LIBLOUIS_TEST_F('testBackTranslateComputerBraille', async function(liblouis) {
  const cells = new Uint8Array([0x53, 0x11, 0x07, 0x07, 0x15, 0x2e]);
  const text = await backTranslate(liblouis, 'en-us-comp8.ctb', cells.buffer);
  assertEquals('Hello!', text);
});

LIBLOUIS_TEST_F('testTranslateGermanGrade2Braille', async function(liblouis) {
  // This is one of the moderately large tables.
  const [cells, textToBraille, brailleToText] =
      await translate(liblouis, 'de-g2.ctb', 'München');
  assertEqualsUint8Array([0x0d, 0x33, 0x1d, 0x39, 0x09], cells);
  assertEqualsJSON([0, 1, 2, 3, 3, 4, 4], textToBraille);
  assertEqualsJSON([0, 1, 2, 3, 5], brailleToText);
});

LIBLOUIS_TEST_F('testTranslateSpaceIsNotDropped', async function(liblouis) {
  const [cells, textToBraille, brailleToText] =
      await translate(liblouis, 'en-ueb-g2.ctb', ' ');
  assertEqualsUint8Array([0x0], cells);
});

LIBLOUIS_TEST_F(
    'testBackTranslateGermanComputerBraille', async function(liblouis) {
      const cells = new Uint8Array([0xb3]);
      const text =
          await backTranslate(liblouis, 'de-de-comp8.ctb', cells.buffer);
      assertEquals('ü', text);
    });

LIBLOUIS_TEST_F(
    'testBackTranslateUSEnglishGrade2PreservesTrailingSpace',
    async function(liblouis) {
      // A full braille cell (dots 1-6) is 'for' when backtranslated.
      const cells = new Uint8Array([0b111111, 0]);
      const text = await backTranslate(liblouis, 'en-ueb-g2.ctb', cells.buffer);
      assertNotEquals(null, text);
      assertEquals('for ', text);
    });

LIBLOUIS_TEST_F('testBackTranslateEmptyCells', async function(liblouis) {
  const text =
      await backTranslate(liblouis, 'de-de-comp8.ctb', new Uint8Array().buffer);
  assertNotEquals(null, text);
  assertEquals(0, text.length);
});

LIBLOUIS_TEST_F('testGetInvalidTranslator', async function(liblouis) {
  console.log('Expecting an error from liblouis');
  const translator = await withTranslator(liblouis, 'nonexistant-table');
  assertEquals(null, translator);
});

LIBLOUIS_TEST_F('testKeyEventStaticData', async function(liblouis) {
  const [cells, textToBraille, brailleToText] = await translate(
      liblouis, 'en-us-comp8.ctb', 'abcdefghijklmnopqrstuvwxyz 0123456789');
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

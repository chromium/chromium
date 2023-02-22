// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '../../testing/chromevox_e2e_test_base.js',
  '../../testing/fake_objects.js',
]);

/**
 * Test fixture.
 */
ChromeVoxBrailleDisplayManagerTest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await importModule(
        'BrailleCaptionsBackground',
        '/chromevox/background/braille/braille_captions_background.js');
    await importModule(
        'BrailleDisplayManager',
        '/chromevox/background/braille/braille_display_manager.js');
    await importModule(
        'BrailleTranslatorManager',
        '/chromevox/background/braille/braille_translator_manager.js');
    await importModule(
        'CURSOR_DOTS', '/chromevox/background/braille/cursor_dots.js');
    await importModule(
        'ExpandingBrailleTranslator',
        '/chromevox/background/braille/expanding_braille_translator.js');
    await importModule(
        'NavBraille', '/chromevox/common/braille/nav_braille.js');
    await importModule('LocalStorage', '/common/local_storage.js');
    await importModule(
        'SettingsManager', '/chromevox/common/settings_manager.js');

    /** @const */
    this.NAV_BRAILLE = new NavBraille({text: 'Hello, world!'});
    this.EMPTY_NAV_BRAILLE = new NavBraille({text: ''});
    this.translator = new FakeTranslator();
    BrailleTranslatorManager.instance = new FakeTranslatorManager();
    /** @const */
    this.DISPLAY_ROW_SIZE = 1;
    this.DISPLAY_COLUMN_SIZE = 12;

    chrome.brailleDisplayPrivate.getDisplayState = callback => {
      callback(this.displayState);
    };
    this.writtenCells = [];
    chrome.brailleDisplayPrivate.writeDots = cells => {
      this.writtenCells.push(cells);
    };
    chrome.brailleDisplayPrivate.onDisplayStateChanged = new FakeChromeEvent();

    this.displayState = {
      available: true,
      textRowCount: this.DISPLAY_ROW_SIZE,
      textColumnCount: this.DISPLAY_COLUMN_SIZE,
    };
  }

  /**
   * Asserts display pan position and selection markers on the last written
   * display content and clears it.  There must be exactly one
   * set of cells written.
   * @param {number} start expected pan position in the braille display
   * @param {number=} opt_selStart first cell (relative to buffer start) that
   *                               should have a selection
   * @param {number=} opt_selEnd last cell that should have a selection.
   */
  assertDisplayPositionAndClear(start, opt_selStart, opt_selEnd) {
    if (opt_selStart !== undefined && opt_selEnd === undefined) {
      opt_selEnd = opt_selStart + 1;
    }
    assertEquals(1, this.writtenCells.length);
    const a = new Uint8Array(this.writtenCells[0]);
    this.writtenCells.length = 0;
    const firstCell = a[0] & ~CURSOR_DOTS;
    // We are asserting that start, which is an index, and firstCell,
    // which is a value, are the same because the fakeTranslator generates
    // the values of the braille cells based on indices.
    assertEquals(
        start, firstCell, ' Start mismatch: ' + start + ' vs. ' + firstCell);
    if (opt_selStart !== undefined) {
      for (let i = opt_selStart; i < opt_selEnd; ++i) {
        assertEquals(
            CURSOR_DOTS, a[i] & CURSOR_DOTS,
            'Missing cursor marker at position ' + i);
      }
    }
  }

  /**
   * Asserts that the last written display content is an empty buffer of
   * of cells and clears the list of written cells.
   * There must be only one buffer in the list.
   */
  assertEmptyDisplayAndClear() {
    assertEquals(1, this.writtenCells.length);
    const content = this.writtenCells[0];
    this.writtenCells.length = 0;
    assertTrue(content instanceof ArrayBuffer);
    assertTrue(content.byteLength === 0);
  }

  /**
   * Asserts that the groups passed in actually match what we expect.
   */
  assertGroupsValid(groups, expected) {
    assertEquals(JSON.stringify(groups), JSON.stringify(expected));
  }

  /**
   * Simulates an onDisplayStateChanged event.
   * @param {{available: boolean, textRowCount: (number|undefined),
   *     textColumnCount: (number|undefined)}}
   */
  simulateOnDisplayStateChanged(event) {
    const listener =
        chrome.brailleDisplayPrivate.onDisplayStateChanged.getListener();
    assertNotEquals(null, listener);
    listener(event);
  }
};

/** @extends {ExpandingBrailleTranslator} */
function FakeTranslator() {}

FakeTranslator.prototype = {
  /**
   * Does a translation where every other character becomes two cells.
   * The translated text does not correspond with the actual content of
   * the original text, but instead uses the indices. Each even index of the
   * original text is mapped to one translated cell, while each odd index is
   * mapped to two translated cells.
   * @override
   */
  translate(spannable, expansionType, callback) {
    text = spannable.toString();
    const buf = new Uint8Array(text.length + text.length / 2);
    const textToBraille = [];
    const brailleToText = [];
    let idx = 0;
    for (let i = 0; i < text.length; ++i) {
      textToBraille.push(idx);
      brailleToText.push(i);
      buf[idx] = idx;
      idx++;
      if ((i % 2) === 1) {
        buf[idx] = idx;
        idx++;
        brailleToText.push(i);
      }
    }
    callback(buf.buffer, textToBraille, brailleToText);
  },
};

/** @extends {BrailleTranslatorManager} */
function FakeTranslatorManager() {}

FakeTranslatorManager.prototype = {
  changeListener: null,
  translator: null,

  setTranslator(translator) {
    this.translator = translator;
    if (this.changeListener) {
      this.changeListener();
    }
  },

  addChangeListener(listener) {
    assertEquals(null, this.changeListener);
    this.changeListener = listener;
  },

  getExpandingTranslator() {
    return this.translator;
  },
};

AX_TEST_F('ChromeVoxBrailleDisplayManagerTest', 'NoApi', function() {
  const manager = new BrailleDisplayManager();
  manager.setContent(this.NAV_BRAILLE);
  BrailleTranslatorManager.instance.setTranslator(this.translator);
  manager.setContent(this.NAV_BRAILLE);
});

/**
 * Test that we don't write to the display when the API is available, but
 * the display is not.
 */
AX_TEST_F('ChromeVoxBrailleDisplayManagerTest', 'NoDisplay', function() {
  this.displayState = {available: false};

  const manager = new BrailleDisplayManager();
  manager.setContent(this.NAV_BRAILLE);
  BrailleTranslatorManager.instance.setTranslator(this.translator);
  manager.setContent(this.NAV_BRAILLE);
  assertEquals(0, this.writtenCells.length);
});

/**
 * Tests the typical sequence: setContent, setTranslator, setContent.
 */
AX_TEST_F('ChromeVoxBrailleDisplayManagerTest', 'BasicSetContent', function() {
  const manager = new BrailleDisplayManager();
  this.assertEmptyDisplayAndClear();
  manager.setContent(this.NAV_BRAILLE);
  this.assertEmptyDisplayAndClear();
  BrailleTranslatorManager.instance.setTranslator(this.translator);
  this.assertDisplayPositionAndClear(0);
  manager.setContent(this.NAV_BRAILLE);
  this.assertDisplayPositionAndClear(0);
});

/**
 * Tests that setting empty content clears the display.
 */
AX_TEST_F(
    'ChromeVoxBrailleDisplayManagerTest', 'SetEmptyContentWithTranslator',
    function() {
      const manager = new BrailleDisplayManager();
      this.assertEmptyDisplayAndClear();
      manager.setContent(this.NAV_BRAILLE);
      this.assertEmptyDisplayAndClear();
      BrailleTranslatorManager.instance.setTranslator(this.translator);
      this.assertDisplayPositionAndClear(0);
      manager.setContent(this.EMPTY_NAV_BRAILLE);
      this.assertEmptyDisplayAndClear();
    });


AX_TEST_F(
    'ChromeVoxBrailleDisplayManagerTest', 'CursorAndPanning', function() {
      const text = 'This is a test string';
      function createNavBrailleWithCursor(start, end) {
        return new NavBraille({text, startIndex: start, endIndex: end});
      }

      const translatedSize = Math.floor(text.length + text.length / 2);

      const manager = new BrailleDisplayManager();
      this.assertEmptyDisplayAndClear();
      BrailleTranslatorManager.instance.setTranslator(this.translator);
      this.assertEmptyDisplayAndClear();

      // Cursor at beginning of line.
      manager.setContent(createNavBrailleWithCursor(0, 0));
      this.assertDisplayPositionAndClear(0, 0);
      // When cursor at end of line.
      manager.setContent(createNavBrailleWithCursor(text.length, text.length));
      // The first braille cell should be the result of the equation below.
      this.assertDisplayPositionAndClear(
          Math.floor(translatedSize / this.DISPLAY_COLUMN_SIZE) *
              this.DISPLAY_COLUMN_SIZE,
          translatedSize % this.DISPLAY_COLUMN_SIZE);
      // Selection from the end of what fits on the first display to the end of
      // the line.
      manager.setContent(createNavBrailleWithCursor(7, text.length));
      this.assertDisplayPositionAndClear(0, 10, this.DISPLAY_COLUMN_SIZE);
      // Selection on all of the line.
      manager.setContent(createNavBrailleWithCursor(0, text.length));
      this.assertDisplayPositionAndClear(0, 0, this.DISPLAY_COLUMN_SIZE);
    });

/**
 * Tests that the grouping algorithm works with one text character that maps
 * to one braille cell.
 */
AX_TEST_F('ChromeVoxBrailleDisplayManagerTest', 'BasicGroup', function() {
  const text = 'a';
  const translated = '1';
  const mapping = [0];
  const expected = [['a', '1']];
  const offsets = {brailleOffset: 0, textOffset: 0};

  const groups = BrailleCaptionsBackground.groupBrailleAndText(
      translated, text, mapping, offsets);
  this.assertGroupsValid(groups, expected);
});

/**
 * Tests that the grouping algorithm works with one text character that maps
 * to multiple braille cells.
 */
AX_TEST_F('ChromeVoxBrailleDisplayManagerTest', 'OneRtoManyB', function() {
  const text = 'A';
  const translated = '11';
  const mapping = [0, 0];
  const expected = [['A', '11']];
  const offsets = {brailleOffset: 0, textOffset: 0};

  const groups = BrailleCaptionsBackground.groupBrailleAndText(
      translated, text, mapping, offsets);
  this.assertGroupsValid(groups, expected);
});

/**
 * Tests that the grouping algorithm works with one braille cell that maps
 * to multiple text characters.
 */
AX_TEST_F('ChromeVoxBrailleDisplayManagerTest', 'OneBtoManyR', function() {
  const text = 'knowledge';
  const translated = '1';
  const mapping = [0];
  const expected = [['knowledge', '1']];
  const offsets = {brailleOffset: 0, textOffset: 0};

  const groups = BrailleCaptionsBackground.groupBrailleAndText(
      translated, text, mapping, offsets);
  this.assertGroupsValid(groups, expected);
});

/**
 * Tests that the grouping algorithm works with one string that on both ends,
 * have text characters that map to multiple braille cells.
 */
AX_TEST_F(
    'ChromeVoxBrailleDisplayManagerTest', 'OneRtoManyB_BothEnds', function() {
      const text = 'AbbC';
      const translated = 'X122X3';
      const mapping = [0, 0, 1, 2, 3, 3];
      const expected = [['A', 'X1'], ['b', '2'], ['b', '2'], ['C', 'X3']];
      const offsets = {brailleOffset: 0, textOffset: 0};

      const groups = BrailleCaptionsBackground.groupBrailleAndText(
          translated, text, mapping, offsets);
      this.assertGroupsValid(groups, expected);
    });

/**
 * Tests that the grouping algorithm works with one string that on both ends,
 * have braille cells that map to multiple text characters.
 */
AX_TEST_F(
    'ChromeVoxBrailleDisplayManagerTest', 'OneBtoManyR_BothEnds', function() {
      const text = 'knowledgehappych';
      const translated = '1234456';
      const mapping = [0, 9, 10, 11, 12, 13, 14];
      const expected = [
        ['knowledge', '1'],
        ['h', '2'],
        ['a', '3'],
        ['p', '4'],
        ['p', '4'],
        ['y', '5'],
        ['ch', '6'],
      ];
      const offsets = {brailleOffset: 0, textOffset: 0};

      const groups = BrailleCaptionsBackground.groupBrailleAndText(
          translated, text, mapping, offsets);
      this.assertGroupsValid(groups, expected);
    });

/**
 * Tests that the grouping algorithm works with one  string that has both types
 * of mapping.
 */
AX_TEST_F('ChromeVoxBrailleDisplayManagerTest', 'RandB_Random', function() {
  const text = 'knowledgeIsPower';
  const translated = '1X23X45678';
  const mapping = [0, 9, 9, 10, 11, 11, 12, 13, 14, 15];
  const expected = [
    ['knowledge', '1'],
    ['I', 'X2'],
    ['s', '3'],
    ['P', 'X4'],
    ['o', '5'],
    ['w', '6'],
    ['e', '7'],
    ['r', '8'],
  ];
  const offsets = {brailleOffset: 0, textOffset: 0};

  const groups = BrailleCaptionsBackground.groupBrailleAndText(
      translated, text, mapping, offsets);
  this.assertGroupsValid(groups, expected);
});

/**
 * Tests that braille-related preferences are updated upon connecting and
 * disconnecting a braille display.
 */
AX_TEST_F('ChromeVoxBrailleDisplayManagerTest', 'UpdatePrefs', function() {
  this.displayState = {available: false};
  const manager = new BrailleDisplayManager();
  assertEquals(false, SettingsManager.get('menuBrailleCommands'));
  this.simulateOnDisplayStateChanged({available: true});
  assertEquals(true, SettingsManager.get('menuBrailleCommands'));
  this.simulateOnDisplayStateChanged({available: false});
  assertEquals(false, SettingsManager.get('menuBrailleCommands'));
});

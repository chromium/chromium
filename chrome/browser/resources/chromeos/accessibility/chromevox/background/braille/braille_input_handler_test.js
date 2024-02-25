// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../../testing/chromevox_e2e_test_base.js']);
GEN_INCLUDE(['../../testing/fake_objects.js']);
GEN_INCLUDE(['../../../common/testing/common.js']);

/**
 * A fake input field that behaves like the Braille IME and also updates
 * the input manager's knowledge about the display content when text changes
 * in the edit field.
 */
FakeEditor = class {
  /**
   * @param {FakePort} port A fake port.
   * @param {BrailleInputHandler} inputHandler to work with.
   */
  constructor(port, inputHandler) {
    /** @private {FakePort} */
    this.port_ = port;
    /** @private {BrailleInputHandler} */
    this.inputHandler_ = inputHandler;
    /** @private {string} */
    this.text_ = '';
    /** @private {number} */
    this.selectionStart_ = 0;
    /** @private {number} */
    this.selectionEnd_ = 0;
    /** @private {number} */
    this.contextID_ = 0;
    /** @private {boolean} */
    this.allowDeletes_ = false;
    /** @private {string} */
    this.uncommittedText_ = '';
    /** @private {?Array<number>} */
    this.extraCells_ = [];
    port.postMessage = message => this.handleMessage_(message);
  }

  /**
   * Sets the content and selection (or cursor) of the edit field.
   * This fakes what happens when the field is edited by other means than
   * via the braille keyboard.
   * @param {string} text Text to replace the current content of the field.
   * @param {number} selectionStart Start of the selection or cursor position.
   * @param {number=} opt_selectionEnd End of selection, or ommited if the
   *     selection is a cursor.
   */
  setContent(text, selectionStart, opt_selectionEnd) {
    this.text_ = text;
    this.selectionStart_ = selectionStart;
    this.selectionEnd_ =
        (opt_selectionEnd !== undefined) ? opt_selectionEnd : selectionStart;
    this.callOnDisplayContentChanged_();
  }

  /**
   * Sets the selection in the editor.
   * @param {number} selectionStart Start of the selection or cursor position.
   * @param {number=} opt_selectionEnd End of selection, or ommited if the
   *     selection is a cursor.
   */
  select(selectionStart, opt_selectionEnd) {
    this.setContent(this.text_, selectionStart, opt_selectionEnd);
  }

  /**
   * Inserts text into the edit field, optionally selecting the inserted
   * text.
   * @param {string} newText Text to insert.
   * @param {boolean=} opt_select If {@code true}, selects the inserted text,
   *     otherwise leaves the cursor at the end of the new text.
   */
  insert(newText, opt_select) {
    this.text_ = this.text_.substring(0, this.selectionStart_) + newText +
        this.text_.substring(this.selectionEnd_);
    if (opt_select) {
      this.selectionEnd_ = this.selectionStart_ + newText.length;
    } else {
      this.selectionStart_ += newText.length;
      this.selectionEnd_ = this.selectionStart_;
    }
    this.callOnDisplayContentChanged_();
  }

  /**
   * Sets whether the editor should cause a test failure if the input handler
   * tries to delete text before the cursor.  By default, thi value is
   * {@code false}.
   * @param {boolean} allowDeletes The new value.
   */
  setAllowDeletes(allowDeletes) {
    this.allowDeletes_ = allowDeletes;
  }

  /**
   * Signals to the input handler that the Braille IME is active or not active,
   * depending on the argument.
   * @param {boolean} value Whether the IME is active or not.
   */
  setActive(value) {
    this.message_({type: 'activeState', active: value});
  }

  /**
   * Fails if the current editor content and selection range don't match
   * the arguments to this function.
   * @param {string} text Text that should be in the field.
   * @param {number} selectionStart Start of selection.
   * @param {number+} opt_selectionEnd End of selection, default to selection
   *     start to indicate a cursor.
   */
  assertContentIs(text, selectionStart, opt_selectionEnd) {
    const selectionEnd =
        (opt_selectionEnd !== undefined) ? opt_selectionEnd : selectionStart;
    assertEquals(text, this.text_);
    assertEquals(selectionStart, this.selectionStart_);
    assertEquals(selectionEnd, this.selectionEnd_);
  }

  /**
   * Asserts that the uncommitted text last sent to the IME is the given text.
   * @param {string} text
   */
  assertUncommittedTextIs(text) {
    assertEquals(text, this.uncommittedText_);
  }

  /**
   * Asserts that the input handler has added 'extra cells' for uncommitted
   * text into the braille content.
   * @param {string} cells Cells as a space-separated list of numbers.
   */
  assertExtraCellsAre(cells) {
    assertEqualsJSON(cellsToArray(cells), this.extraCells_);
  }

  /**
   * Sends a message from the IME to the input handler.
   * @param {Object} msg The message to send.
   * @private
   */
  message_(msg) {
    const listener = this.port_.onMessage.getListener();
    assertNotEquals(null, listener);
    listener(msg);
  }

  createValue(text, opt_selStart, opt_selEnd, opt_textOffset) {
    const spannable = new Spannable(text, new ValueSpan(opt_textOffset || 0));
    if (opt_selStart !== undefined) {
      opt_selEnd = (opt_selEnd !== undefined) ? opt_selEnd : opt_selStart;
      // TODO(plundblad): This looses the distinction between the selection
      // anchor (start) and focus (end).  We should use that information to
      // decide where to pan the braille display.
      if (opt_selStart > opt_selEnd) {
        const temp = opt_selStart;
        opt_selStart = opt_selEnd;
        opt_selEnd = temp;
      }

      spannable.setSpan(new ValueSelectionSpan(), opt_selStart, opt_selEnd);
    }
    return spannable;
  }

  /**
   * Calls the {@code onDisplayContentChanged} method of the input handler
   * with the current editor content and selection.
   * @private
   */
  callOnDisplayContentChanged_() {
    const content =
        this.createValue(this.text_, this.selectionStart_, this.selectionEnd_);
    const grabExtraCells = () => {
      const span = content.getSpanInstanceOf(ExtraCellsSpan);
      assertNotEquals(null, span);
      // Convert the ArrayBuffer to a normal array for easier comparison.
      this.extraCells_ = Array.from(new Uint8Array(span.cells));
    };
    this.inputHandler_.onDisplayContentChanged(content, grabExtraCells);
    grabExtraCells();
  }

  /**
   * Informs the input handler that a new text field is focused.  The content
   * of the field is not cleared and should be updated separately.
   * @param {string} fieldType The type of the field (see the documentation
   *     for the {@code chrome.input.ime} API).
   */
  focus(fieldType) {
    this.contextID_++;
    this.message_({
      type: 'inputContext',
      context: {type: fieldType, contextID: this.contextID_},
    });
  }

  /**
   * Inform the input handler that focus left the input field.
   */
  blur() {
    this.message_({type: 'inputContext', context: null});
    this.contextID_ = 0;
  }

  /**
   * Handles a message from the input handler to the IME.
   * @param {Object} msg The message.
   * @private
   */
  handleMessage_(msg) {
    assertEquals(this.contextID_, msg.contextID);
    switch (msg.type) {
      case 'replaceText':
        const deleteBefore = msg.deleteBefore;
        const newText = msg.newText;
        assertTrue(goog.isNumber(deleteBefore));
        assertTrue(goog.isString(newText));
        assertTrue(deleteBefore <= this.selectionStart_);
        if (deleteBefore > 0) {
          assertTrue(this.allowDeletes_);
          this.text_ =
              this.text_.substring(0, this.selectionStart_ - deleteBefore) +
              this.text_.substring(this.selectionEnd_);
          this.selectionStart_ -= deleteBefore;
          this.selectionEnd_ = this.selectionStart_;
          this.callOnDisplayContentChanged_();
        }
        this.insert(newText);
        break;
      case 'setUncommitted':
        assertTrue(goog.isString(msg.text));
        this.uncommittedText_ = msg.text;
        break;
      case 'commitUncommitted':
        this.insert(this.uncommittedText_);
        this.uncommittedText_ = '';
        break;
      default:
        throw new Error('Unexpected message to IME: ' + JSON.stringify(msg));
    }
  }
};


/*
 * Fakes a {@code Port} used for message passing in the Chrome extension APIs.
 * @constructor
 */
function FakePort() {
  /** @type {FakeChromeEvent} */
  this.onDisconnect = new FakeChromeEvent();
  /** @type {FakeChromeEvent} */
  this.onMessage = new FakeChromeEvent();
  /** @type {string} */
  this.name = BrailleInputHandler.IME_PORT_NAME_;
  /** @type {{id: string}} */
  this.sender = {id: BrailleInputHandler.IME_EXTENSION_ID_};
}

/**
 * Mapping from braille cells to Unicode characters.
 * @const Array<Array<string> >
 */
const UNCONTRACTED_TABLE = [
  ['0', ' '],    ['1', 'a'],    ['12', 'b'],    ['14', 'c'],   ['145', 'd'],
  ['15', 'e'],   ['124', 'f'],  ['1245', 'g'],  ['125', 'h'],  ['24', 'i'],
  ['245', 'j'],  ['13', 'k'],   ['123', 'l'],   ['134', 'm'],  ['1345', 'n'],
  ['135', 'o'],  ['1234', 'p'], ['12345', 'q'], ['1235', 'r'], ['234', 's'],
  ['2345', 't'],
];


/**
 * Mapping of braille cells to the corresponding word in Grade 2 US English
 * braille.  This table also includes the uncontracted table above.
 * If a match 'pattern' starts with '^', it must be at the beginning of
 * the string or be preceded by a blank cell.  Similarly, '$' at the end
 * of a 'pattern' means that the match must be at the end of the string
 * or be followed by a blank cell.  Note that order is significant in the
 * table.  First match wins.
 * @const
 */
const CONTRACTED_TABLE = [
  ['12 1235 123', 'braille'],
  ['^12$', 'but'],
  ['1456', 'this'],
].concat(UNCONTRACTED_TABLE);

/**
 * A fake braille translator that can do back translation according
 * to one of the tables above.
 */
FakeTranslator = class {
  /**
   * @param {Array<Array<number>>} table Backtranslation mapping.
   * @param {boolean=} opt_capitalize Whether the result should be capitalized.
   */
  constructor(table, opt_capitalize) {
    /** @private */
    this.table_ = table.map(entry => {
      let cells = entry[0];
      const result = [];
      if (cells[0] === '^') {
        result.start = true;
        cells = cells.substring(1);
      }
      if (cells[cells.length - 1] === '$') {
        result.end = true;
        cells = cells.substring(0, cells.length - 1);
      }
      result[0] = cellsToArray(cells);
      result[1] = entry[1];
      return result;
    });
    /** @private {boolean} */
    this.capitalize_ = opt_capitalize || false;
  }

  /**
   * Implements the {@code LibLouis.BrailleTranslator.backTranslate} method.
   * @param {!ArrayBuffer} cells Cells to be translated.
   * @param {function(?string)} callback Callback for result.
   */
  backTranslate(cells, callback) {
    const cellsArray = new Uint8Array(cells);
    let result = '';
    let pos = 0;
    while (pos < cellsArray.length) {
      let match = null;
      outer: for (let i = 0, j, entry; entry = this.table_[i]; ++i) {
        if (pos + entry[0].length > cellsArray.length) {
          continue;
        }
        if (entry.start && pos > 0 && cellsArray[pos - 1] !== 0) {
          continue;
        }
        for (j = 0; j < entry[0].length; ++j) {
          if (entry[0][j] !== cellsArray[pos + j]) {
            continue outer;
          }
        }
        if (entry.end && pos + j < cellsArray.length &&
            cellsArray[pos + j] !== 0) {
          continue;
        }
        match = entry;
        break;
      }
      assertNotEquals(
          null, match, 'Backtranslating ' + cellsArray[pos] + ' at ' + pos);
      result += match[1];
      pos += match[0].length;
    }
    if (this.capitalize_) {
      result = result.toUpperCase();
    }
    callback(result);
  }
};


/** @extends {BrailleTranslatorManager} */
function FakeTranslatorManager() {}

FakeTranslatorManager.prototype = {
  defaultTranslator: null,
  uncontractedTranslator: null,
  changeListener: null,

  /** @override */
  getDefaultTranslator() {
    return this.defaultTranslator;
  },

  /** @override */
  getUncontractedTranslator() {
    return this.uncontractedTranslator;
  },

  /** @override */
  addChangeListener(listener) {
    assertEquals(null, this.changeListener);
  },

  setTranslators(defaultTranslator, uncontractedTranslator) {
    this.defaultTranslator = defaultTranslator;
    this.uncontractedTranslator = uncontractedTranslator;
    if (this.changeListener) {
      this.changeListener();
    }
  },
};

/**
 * Converts a list of cells, represented as a string, to an array.
 * @param {string} cells A string with space separated groups of digits.
 *     Each group corresponds to one braille cell and each digit in a group
 *     corresponds to a particular dot in the cell (1 to 8).  As a special
 *     case, the digit 0 by itself represents a blank cell.
 * @return {Array<number>} An array with each cell encoded as a bit
 *     pattern (dot 1 uses bit 0, etc).
 */
function cellsToArray(cells) {
  if (!cells) {
    return [];
  }
  return cells.split(/\s+/).map(cellString => {
    let cell = 0;
    assertTrue(cellString.length > 0);
    if (cellString !== '0') {
      for (let i = 0; i < cellString.length; ++i) {
        const dot = cellString.charCodeAt(i) - '0'.charCodeAt(0);
        assertTrue(dot >= 1);
        assertTrue(dot <= 8);
        cell |= 1 << (dot - 1);
      }
    }
    return cell;
  });
}

/**
 * Test fixture.
 */
ChromeVoxBrailleInputHandlerTest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    chrome.runtime.onConnectExternal = new FakeChromeEvent();
    this.port = new FakePort();
    chrome.accessibilityPrivate.sendSyntheticKeyEvent =
        (event, useRewriters, opt_callback) =>
            this.storeKeyEvent(event, useRewriters, opt_callback);
    chrome.accessibilityPrivate.SyntheticKeyboardEventType = {};
    chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN = 'keydown';
    chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYUP = 'keyup';
    BrailleTranslatorManager.instance = new FakeTranslatorManager();
    this.inputHandler = new BrailleInputHandler();
    this.uncontractedTranslator = new FakeTranslator(UNCONTRACTED_TABLE);
    this.contractedTranslator = new FakeTranslator(CONTRACTED_TABLE, true);
    this.keyEvents = [];
  }

  /**
   * Creates an editor and establishes a connection from the IME.
   * @return {FakeEditor}
   */
  createEditor() {
    chrome.runtime.onConnectExternal.getListener()(this.port);
    return new FakeEditor(this.port, this.inputHandler);
  }

  /**
   * Sends a series of braille cells to the input handler.
   * @param {string} cells Braille cells, encoded as described in
   *     {@code cellsToArray}.
   * @return {boolean} {@code true} iff all cells were sent successfully.
   */
  sendCells(cells) {
    return cellsToArray(cells).reduce((prevResult, cell) => {
      const event = {command: BrailleKeyCommand.DOTS, brailleDots: cell};
      return prevResult && this.inputHandler.onBrailleKeyEvent(event);
    }, true);
  }

  /**
   * Sends a standard key event (such as backspace) to the braille input
   * handler.
   * @param {string} keyCode The key code name.
   * @return {boolean} Whether the event was handled.
   */
  sendKeyEvent(keyCode) {
    const event = {
      command: BrailleKeyCommand.STANDARD_KEY,
      standardKeyCode: keyCode,
    };
    return this.inputHandler.onBrailleKeyEvent(event);
  }

  /**
   * Shortcut for asserting that the value expansion mode is {@code NONE}.
   */
  assertExpandingNone() {
    assertEquals(
        ExpandingBrailleTranslator.ExpansionType.NONE,
        this.inputHandler.getExpansionType());
  }

  /**
   * Shortcut for asserting that the value expansion mode is {@code SELECTION}.
   */
  assertExpandingSelection() {
    assertEquals(
        ExpandingBrailleTranslator.ExpansionType.SELECTION,
        this.inputHandler.getExpansionType());
  }

  /**
   * Shortcut for asserting that the value expansion mode is {@code ALL}.
   */
  assertExpandingAll() {
    assertEquals(
        ExpandingBrailleTranslator.ExpansionType.ALL,
        this.inputHandler.getExpansionType());
  }

  storeKeyEvent(event, useRewriters, opt_callback) {
    const storedCopy = {keyCode: event.keyCode};
    if (event.type === 'keydown') {
      this.keyEvents.push(storedCopy);
    } else {
      assertEquals('keyup', event.type);
      assertTrue(this.keyEvents.length > 0);
      assertEqualsJSON(storedCopy, this.keyEvents[this.keyEvents.length - 1]);
    }
    if (opt_callback !== undefined) {
      callback();
    }
  }
};

AX_TEST_F(
    'ChromeVoxBrailleInputHandlerTest', 'ConnectFromUnknownExtension',
    function() {
      this.port.sender.id = 'your unknown friend';
      chrome.runtime.onConnectExternal.getListener()(this.port);
      this.port.onMessage.assertNoListener();
    });

AX_TEST_F('ChromeVoxBrailleInputHandlerTest', 'NoTranslator', function() {
  const editor = this.createEditor();
  editor.setContent('blah', 0);
  editor.setActive(true);
  editor.focus('email');
  assertFalse(this.sendCells('145 135 125'));
  editor.setActive(false);
  editor.blur();
  editor.assertContentIs('blah', 0);
});

AX_TEST_F('ChromeVoxBrailleInputHandlerTest', 'InputUncontracted', function() {
  BrailleTranslatorManager.instance.setTranslators(
      this.uncontractedTranslator, null);
  const editor = this.createEditor();
  editor.setActive(true);

  // Focus and type in a text field.
  editor.focus('text');
  assertTrue(this.sendCells('125 15 123 123 135'));  // hello
  editor.assertContentIs('hello', 'hello'.length);
  this.assertExpandingNone();

  // Move the cursor and type in the middle.
  editor.select(2);
  assertTrue(this.sendCells('0 2345 125 15 1235 15 0'));  // ' there '
  editor.assertContentIs('he there llo', 'he there '.length);

  // Field changes by some other means.
  editor.insert('you!');
  // Then type on the braille keyboard again.
  assertTrue(this.sendCells('0 125 15'));  // ' he'
  editor.assertContentIs('he there you! hello', 'he there you! he'.length);

  editor.blur();
  editor.setActive(false);
});

AX_TEST_F('ChromeVoxBrailleInputHandlerTest', 'InputContracted', function() {
  const editor = this.createEditor();
  BrailleTranslatorManager.instance.setTranslators(
      this.contractedTranslator, this.uncontractedTranslator);
  editor.setContent('', 0);
  editor.setActive(true);
  editor.focus('text');
  this.assertExpandingSelection();

  // First, type a 'b'.
  assertTrue(this.sendCells('12'));
  editor.assertContentIs('', 0);
  // Remember that the contracted translator produces uppercase.
  editor.assertUncommittedTextIs('BUT');
  editor.assertExtraCellsAre('12');
  this.assertExpandingNone();

  // Typing 'rl' changes to a different contraction.
  assertTrue(this.sendCells('1235 123'));
  editor.assertUncommittedTextIs('BRAILLE');
  editor.assertContentIs('', 0);
  editor.assertExtraCellsAre('12 1235 123');
  this.assertExpandingNone();

  // Now, finish the word.
  assertTrue(this.sendCells('0'));
  editor.assertContentIs('BRAILLE ', 'BRAILLE '.length);
  editor.assertUncommittedTextIs('');
  editor.assertExtraCellsAre('');
  this.assertExpandingSelection();

  // Move the cursor to the beginning.
  editor.select(0);
  this.assertExpandingSelection();

  // Typing now uses the uncontracted table.
  assertTrue(this.sendCells('12'));  // 'b'
  editor.assertContentIs('bBRAILLE ', 1);
  this.assertExpandingSelection();
  editor.select('bBRAILLE'.length);
  this.assertExpandingSelection();
  assertTrue(this.sendCells('12'));  // 'b'
  editor.assertContentIs('bBRAILLEb ', 'bBRAILLEb'.length);
  // Move to the end, where contracted typing should work.
  editor.select('bBRAILLEb '.length);
  assertTrue(this.sendCells('1456 0'));  // Symbol for 'this', then space.
  this.assertExpandingSelection();
  editor.assertContentIs('bBRAILLEb THIS ', 'bBRAILLEb THIS '.length);

  // Move to between the two words.
  editor.select('bBRAILLEb'.length);
  this.assertExpandingSelection();
  assertTrue(this.sendCells('0 12'));  // Space plus 'b' for 'but'
  editor.assertUncommittedTextIs('BUT');
  editor.assertExtraCellsAre('12');
  editor.assertContentIs('bBRAILLEb  THIS ', 'bBRAILLEb '.length);
  this.assertExpandingNone();
});

AX_TEST_F(
    'ChromeVoxBrailleInputHandlerTest', 'TypingUrlWithContracted', function() {
      const editor = this.createEditor();
      BrailleTranslatorManager.instance.setTranslators(
          this.contractedTranslator, this.uncontractedTranslator);
      editor.setActive(true);
      editor.focus('url');
      this.assertExpandingAll();
      assertTrue(this.sendCells('1245'));  // 'g'
      editor.insert('oogle.com', true /*select*/);
      editor.assertContentIs('google.com', 1, 'google.com'.length);
      this.assertExpandingAll();
      this.sendCells('135');  // 'o'
      editor.insert('ogle.com', true /*select*/);
      editor.assertContentIs('google.com', 2, 'google.com'.length);
      this.assertExpandingAll();
      this.sendCells('0');
      editor.assertContentIs('go ', 'go '.length);
      // In a URL, even when the cursor is in whitespace, all of the value
      // is expanded to uncontracted braille.
      this.assertExpandingAll();
    });

AX_TEST_F('ChromeVoxBrailleInputHandlerTest', 'Backspace', function() {
  const editor = this.createEditor();
  BrailleTranslatorManager.instance.setTranslators(
      this.contractedTranslator, this.uncontractedTranslator);
  editor.setActive(true);
  editor.focus('text');

  // Add some text that we can delete later.
  editor.setContent('Text ', 'Text '.length);

  // Type 'brl' to make sure replacement works when deleting text.
  assertTrue(this.sendCells('12 1235 123'));
  editor.assertUncommittedTextIs('BRAILLE');

  // Delete what we just typed, one cell at a time.
  this.sendKeyEvent('Backspace');
  editor.assertUncommittedTextIs('BR');
  this.sendKeyEvent('Backspace');
  editor.assertUncommittedTextIs('BUT');
  this.sendKeyEvent('Backspace');
  editor.assertUncommittedTextIs('');

  // Now, backspace should be handled as usual, synthetizing key events.
  assertEquals(0, this.keyEvents.length);
  this.sendKeyEvent('Backspace');
  assertEqualsJSON([{keyCode: KeyCode.BACK}], this.keyEvents);
});

AX_TEST_F('ChromeVoxBrailleInputHandlerTest', 'KeysImeNotActive', function() {
  const editor = this.createEditor();
  this.sendKeyEvent('Enter');
  this.sendKeyEvent('ArrowUp');
  assertEqualsJSON(
      [{keyCode: KeyCode.RETURN}, {keyCode: KeyCode.UP}], this.keyEvents);
});

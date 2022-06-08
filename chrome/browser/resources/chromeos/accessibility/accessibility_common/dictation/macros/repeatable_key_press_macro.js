// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Macro, MacroError} from '/accessibility_common/dictation/macros/macro.js';
import {MacroName} from '/accessibility_common/dictation/macros/macro_names.js';
import {EventGenerator} from '/common/event_generator.js';

/**
 * Abstract class that executes a macro using a key press which can optionally
 * be repeated.
 * @abstract
 */
export class RepeatableKeyPressMacro extends Macro {
  /**
   * @param {MacroName} macroName The name of the macro.
   * @param {string|number} repeat The number of times to repeat the key press.
   *     May be 3 or '3'.
   */
  constructor(macroName, repeat) {
    super(macroName);

    /** @private {number} */
    this.repeat_ = parseInt(repeat, /*base=*/ 10);
  }

  /** @override */
  checkContext() {
    if (isNaN(this.repeat_)) {
      // This might occur if the numbers grammar did not recognize the
      // spoken number, so we could get a string like "three" instead of
      // the number 3.
      return this.createFailureCheckContextResult_(
          MacroError.INVALID_USER_INTENT);
    }
    // TODO(crbug.com/1264544): Actually check the context and make this
    // abstract.
    return this.createSuccessCheckContextResult_(
        /*willImmediatelyDisambiguate=*/ false);
  }

  /** @override */
  runMacro() {
    for (let i = 0; i < this.repeat_; i++) {
      this.doKeyPress();
    }
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }

  /** @abstract */
  doKeyPress() {}
}

/** Macro to delete by character. */
export class DeletePreviousCharacterMacro extends RepeatableKeyPressMacro {
  /**
   * @param {number=} repeat The number of characters to delete.
   */
  constructor(repeat = 1) {
    super(MacroName.DELETE_PREV_CHAR, repeat);
  }

  /** @override */
  doKeyPress() {
    EventGenerator.sendKeyPress(KeyCode.BACK);
  }
}

/** Macro to navigate to the previous character. */
export class NavPreviousCharMacro extends RepeatableKeyPressMacro {
  /**
   * @param {boolean} isRTLLocale Whether the Dictation speech recognition
   *     locale is right-to-left.
   * @param {number=} repeat The number of characters to move.
   */
  constructor(isRTLLocale, repeat = 1) {
    super(MacroName.NAV_PREV_CHAR, repeat);

    /** @private {boolean} */
    this.isRTLLocale_ = isRTLLocale;
  }

  /** @override */
  doKeyPress() {
    EventGenerator.sendKeyPress(
        this.isRTLLocale_ ? KeyCode.RIGHT : KeyCode.LEFT);
  }
}

/** Macro to navigate to the next character. */
export class NavNextCharMacro extends RepeatableKeyPressMacro {
  /**
   * @param {boolean} isRTLLocale Whether the Dictation speech recognition
   *     locale is right-to-left.
   * @param {number=} repeat The number of characters to move.
   */
  constructor(isRTLLocale, repeat = 1) {
    super(MacroName.NAV_NEXT_CHAR, repeat);

    /** @private {boolean} */
    this.isRTLLocale_ = isRTLLocale;
  }

  /** @override */
  doKeyPress() {
    EventGenerator.sendKeyPress(
        this.isRTLLocale_ ? KeyCode.LEFT : KeyCode.RIGHT);
  }
}

/** Macro to navigate to the previous line. */
export class NavPreviousLineMacro extends RepeatableKeyPressMacro {
  /** @param {number=} repeat The number of lines to move. */
  constructor(repeat = 1) {
    super(MacroName.NAV_PREV_LINE, repeat);
  }

  /** @override */
  doKeyPress() {
    EventGenerator.sendKeyPress(KeyCode.UP);
  }
}

/** Macro to navigate to the next line. */
export class NavNextLineMacro extends RepeatableKeyPressMacro {
  /** @param {number=} repeat The number of lines to move. */
  constructor(repeat = 1) {
    super(MacroName.NAV_NEXT_LINE, repeat);
  }

  /** @override */
  doKeyPress() {
    EventGenerator.sendKeyPress(KeyCode.DOWN);
  }
}

/** Macro to copy selected text. */
export class CopySelectedTextMacro extends RepeatableKeyPressMacro {
  constructor() {
    super(MacroName.COPY_SELECTED_TEXT, /*repeat=*/ 1);
  }

  /** @override */
  doKeyPress() {
    EventGenerator.sendKeyPress(KeyCode.C, {ctrl: true});
  }
}

/** Macro to paste text. */
export class PasteTextMacro extends RepeatableKeyPressMacro {
  constructor() {
    super(MacroName.PASTE_TEXT, /*repeat=*/ 1);
  }

  /** @override */
  doKeyPress() {
    EventGenerator.sendKeyPress(KeyCode.V, {ctrl: true});
  }
}

/** Macro to cut selected text. */
export class CutSelectedTextMacro extends RepeatableKeyPressMacro {
  constructor() {
    super(MacroName.CUT_SELECTED_TEXT, /*repeat=*/ 1);
  }

  /** @override */
  doKeyPress() {
    EventGenerator.sendKeyPress(KeyCode.X, {ctrl: true});
  }
}

/** Macro to undo a text editing action. */
export class UndoTextEditMacro extends RepeatableKeyPressMacro {
  constructor() {
    super(MacroName.UNDO_TEXT_EDIT, /*repeat=*/ 1);
  }

  /** @override */
  doKeyPress() {
    EventGenerator.sendKeyPress(KeyCode.Z, {ctrl: true});
  }
}

/** Macro to redo a text editing action. */
export class RedoActionMacro extends RepeatableKeyPressMacro {
  constructor() {
    super(MacroName.REDO_ACTION, /*repeat=*/ 1);
  }

  /** @override */
  doKeyPress() {
    EventGenerator.sendKeyPress(KeyCode.Z, {ctrl: true, shift: true});
  }
}

/** Macro to select all text. */
export class SelectAllTextMacro extends RepeatableKeyPressMacro {
  constructor() {
    super(MacroName.SELECT_ALL_TEXT, /*repeat=*/ 1);
  }

  /** @override */
  doKeyPress() {
    EventGenerator.sendKeyPress(KeyCode.A, {ctrl: true});
  }
}

/** Macro to unselect text. */
export class UnselectTextMacro extends RepeatableKeyPressMacro {
  /**
   * @param {boolean} isRTLLocale Whether the Dictation speech recognition
   *     locale is right-to-left.
   */
  constructor(isRTLLocale) {
    super(MacroName.UNSELECT_TEXT, /*repeat=*/ 1);
    /** @private {boolean} */
    this.isRTLLocale_ = isRTLLocale;
  }

  /** @override */
  doKeyPress() {
    EventGenerator.sendKeyPress(
        this.isRTLLocale_ ? KeyCode.LEFT : KeyCode.RIGHT);
  }
}

/** Macro to delete the previous word. */
export class DeletePrevWordMacro extends RepeatableKeyPressMacro {
  constructor() {
    super(MacroName.DELETE_PREV_WORD, /*repeat=*/ 1);
  }

  /** @override */
  doKeyPress() {
    EventGenerator.sendKeyPress(KeyCode.BACK, {ctrl: true});
  }
}

/** Macro to navigate to the next word. */
export class NavNextWordMacro extends RepeatableKeyPressMacro {
  /**
   * @param {boolean} isRTLLocale Whether the Dictation speech recognition
   *     locale is right-to-left.
   * @param {number=} repeat The number of words to move.
   */
  constructor(isRTLLocale, repeat = 1) {
    super(MacroName.NAV_NEXT_WORD, repeat);

    /** @private {boolean} */
    this.isRTLLocale_ = isRTLLocale;
  }

  /** @override */
  doKeyPress() {
    EventGenerator.sendKeyPress(
        this.isRTLLocale_ ? KeyCode.LEFT : KeyCode.RIGHT, {ctrl: true});
  }
}

/** Macro to navigate to the previous word. */
export class NavPrevWordMacro extends RepeatableKeyPressMacro {
  /**
   * @param {boolean} isRTLLocale Whether the Dictation speech recognition
   *     locale is right-to-left.
   * @param {number=} repeat The number of words to move.
   */
  constructor(isRTLLocale, repeat = 1) {
    super(MacroName.NAV_PREV_WORD, repeat);

    /** @private {boolean} */
    this.isRTLLocale_ = isRTLLocale;
  }

  /** @override */
  doKeyPress() {
    EventGenerator.sendKeyPress(
        this.isRTLLocale_ ? KeyCode.RIGHT : KeyCode.LEFT, {ctrl: true});
  }
}

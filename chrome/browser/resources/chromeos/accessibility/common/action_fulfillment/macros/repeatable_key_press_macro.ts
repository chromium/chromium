// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventGenerator} from '../../event_generator.js';
import {KeyCode} from '../../key_code.js';
import {TestImportManager} from '../../testing/test_import_manager.js';
import {Context, ContextChecker} from '../context_checker.js';
import {InputController} from '../input_controller.js';

import {CheckContextResult, Macro, MacroError, RunMacroResult} from './macro.js';
import {MacroName} from './macro_names.js';

/**
 * Abstract class that executes a macro using a key press which can optionally
 * be repeated.
 * @abstract
 */
export class RepeatableKeyPressMacro extends Macro {
  private repeat_: number;
  /**
   * @param macroName The name of the macro.
   * @param repeat The number of times to repeat the key press. May be 3 or '3'.
   */
  constructor(
      macroName: MacroName, repeat: string|number, checker?: ContextChecker) {
    super(macroName, checker);

    this.repeat_ = parseInt(String(repeat), /*base=*/ 10);
  }

  override checkContext(): CheckContextResult {
    const checkContextResult = super.checkContext();
    if (!checkContextResult.canTryAction) {
      return checkContextResult;
    }

    if (isNaN(this.repeat_)) {
      // This might occur if the numbers grammar did not recognize the
      // spoken number, so we could get a string like "three" instead of
      // the number 3.
      return this.createFailureCheckContextResult_(
          MacroError.INVALID_USER_INTENT, Context.INVALID_INPUT);
    }

    return this.createSuccessCheckContextResult_();
  }

  override run(): RunMacroResult {
    for (let i = 0; i < this.repeat_; i++) {
      this.doKeyPress();
    }
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }

  doKeyPress(): void {}
}

/** Macro to delete by character. */
export class DeletePreviousCharacterMacro extends RepeatableKeyPressMacro {
  /** @param repeat The number of characters to delete. */
  constructor(inputController: InputController, repeat: number = 1) {
    super(
        MacroName.DELETE_PREV_CHAR, repeat,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
  }

  override doKeyPress(): void {
    EventGenerator.sendKeyPress(KeyCode.BACK);
  }
}

/** Macro to navigate to the previous character. */
export class NavPreviousCharMacro extends RepeatableKeyPressMacro {
  private isRTLLocale_: boolean;
  /** @param repeat The number of characters to move. */
  constructor(
      inputController: InputController, isRTLLocale: boolean,
      repeat: number = 1) {
    super(
        MacroName.NAV_PREV_CHAR, repeat,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
    this.isRTLLocale_ = isRTLLocale;
  }

  override doKeyPress(): void {
    EventGenerator.sendKeyPress(
        this.isRTLLocale_ ? KeyCode.RIGHT : KeyCode.LEFT);
  }
}

/** Macro to navigate to the next character. */
export class NavNextCharMacro extends RepeatableKeyPressMacro {
  private isRTLLocale_: boolean;
  /** @param repeat The number of characters to move. */
  constructor(
      inputController: InputController, isRTLLocale: boolean,
      repeat: number = 1) {
    super(
        MacroName.NAV_NEXT_CHAR, repeat,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
    this.isRTLLocale_ = isRTLLocale;
  }

  override doKeyPress(): void {
    EventGenerator.sendKeyPress(
        this.isRTLLocale_ ? KeyCode.LEFT : KeyCode.RIGHT);
  }
}

/** Macro to navigate to the previous line. */
export class NavPreviousLineMacro extends RepeatableKeyPressMacro {
  /** @param repeat The number of lines to move. */
  constructor(inputController: InputController, repeat: number = 1) {
    super(
        MacroName.NAV_PREV_LINE, repeat,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
  }

  override doKeyPress(): void {
    EventGenerator.sendKeyPress(KeyCode.UP);
  }
}

/** Macro to navigate to the next line. */
export class NavNextLineMacro extends RepeatableKeyPressMacro {
  /** @param repeat The number of lines to move. */
  constructor(inputController: InputController, repeat: number = 1) {
    super(
        MacroName.NAV_NEXT_LINE, repeat,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
  }

  override doKeyPress(): void {
    EventGenerator.sendKeyPress(KeyCode.DOWN);
  }
}

/** Macro to copy selected text. */
export class CopySelectedTextMacro extends RepeatableKeyPressMacro {
  constructor(inputController: InputController) {
    super(
        MacroName.COPY_SELECTED_TEXT, /*repeat=*/ 1,
        new ContextChecker(inputController)
            .add(Context.EMPTY_EDITABLE)
            .add(Context.NO_SELECTION));
  }

  override doKeyPress(): void {
    EventGenerator.sendKeyPress(KeyCode.C, {ctrl: true});
  }
}

/** Macro to paste text. */
export class PasteTextMacro extends RepeatableKeyPressMacro {
  constructor() {
    super(MacroName.PASTE_TEXT, /*repeat=*/ 1);
  }

  override doKeyPress(): void {
    EventGenerator.sendKeyPress(KeyCode.V, {ctrl: true});
  }
}

/** Macro to cut selected text. */
export class CutSelectedTextMacro extends RepeatableKeyPressMacro {
  constructor(inputController: InputController) {
    super(
        MacroName.CUT_SELECTED_TEXT, /*repeat=*/ 1,
        new ContextChecker(inputController)
            .add(Context.EMPTY_EDITABLE)
            .add(Context.NO_SELECTION));
  }

  override doKeyPress(): void {
    EventGenerator.sendKeyPress(KeyCode.X, {ctrl: true});
  }
}

/** Macro to undo a text editing action. */
export class UndoTextEditMacro extends RepeatableKeyPressMacro {
  constructor() {
    super(MacroName.UNDO_TEXT_EDIT, /*repeat=*/ 1);
  }

  override doKeyPress(): void {
    EventGenerator.sendKeyPress(KeyCode.Z, {ctrl: true});
  }
}

/** Macro to redo a text editing action. */
export class RedoActionMacro extends RepeatableKeyPressMacro {
  constructor() {
    super(MacroName.REDO_ACTION, /*repeat=*/ 1);
  }

  override doKeyPress(): void {
    EventGenerator.sendKeyPress(KeyCode.Z, {ctrl: true, shift: true});
  }
}

/** Macro to select all text. */
export class SelectAllTextMacro extends RepeatableKeyPressMacro {
  constructor(inputController: InputController) {
    super(
        MacroName.SELECT_ALL_TEXT, /*repeat=*/ 1,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
  }

  override doKeyPress(): void {
    EventGenerator.sendKeyPress(KeyCode.A, {ctrl: true});
  }
}

/** Macro to unselect text. */
export class UnselectTextMacro extends RepeatableKeyPressMacro {
  private isRTLLocale_: boolean;
  constructor(inputController: InputController, isRTLLocale: boolean) {
    super(
        MacroName.UNSELECT_TEXT, /*repeat=*/ 1,
        new ContextChecker(inputController)
            .add(Context.EMPTY_EDITABLE)
            .add(Context.NO_SELECTION));
    this.isRTLLocale_ = isRTLLocale;
  }

  override doKeyPress(): void {
    EventGenerator.sendKeyPress(
        this.isRTLLocale_ ? KeyCode.LEFT : KeyCode.RIGHT);
  }
}

/** Macro to delete the previous word. */
export class DeletePrevWordMacro extends RepeatableKeyPressMacro {
  /** @param repeat The number of words to delete. */
  constructor(inputController: InputController, repeat = 1) {
    super(
        MacroName.DELETE_PREV_WORD, repeat,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
  }

  override doKeyPress(): void {
    EventGenerator.sendKeyPress(KeyCode.BACK, {ctrl: true});
  }
}

/** Macro to navigate to the next word. */
export class NavNextWordMacro extends RepeatableKeyPressMacro {
  private isRTLLocale_: boolean;
  /** @param repeat The number of words to move. */
  constructor(
      inputController: InputController, isRTLLocale: boolean, repeat = 1) {
    super(
        MacroName.NAV_NEXT_WORD, repeat,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
    this.isRTLLocale_ = isRTLLocale;
  }

  override doKeyPress(): void {
    EventGenerator.sendKeyPress(
        this.isRTLLocale_ ? KeyCode.LEFT : KeyCode.RIGHT, {ctrl: true});
  }
}

/** Macro to navigate to the previous word. */
export class NavPrevWordMacro extends RepeatableKeyPressMacro {
  private isRTLLocale_: boolean;
  /** @param repeat The number of words to move. */
  constructor(
      inputController: InputController, isRTLLocale: boolean, repeat = 1) {
    super(
        MacroName.NAV_PREV_WORD, repeat,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
    this.isRTLLocale_ = isRTLLocale;
  }

  override doKeyPress(): void {
    EventGenerator.sendKeyPress(
        this.isRTLLocale_ ? KeyCode.RIGHT : KeyCode.LEFT, {ctrl: true});
  }
}

/** Macro to delete all text in input field. */
export class DeleteAllText extends RepeatableKeyPressMacro {
  constructor(inputController: InputController) {
    super(
        MacroName.DELETE_ALL_TEXT, 1,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
  }

  override doKeyPress(): void {
    EventGenerator.sendKeyPress(KeyCode.A, {ctrl: true});
    EventGenerator.sendKeyPress(KeyCode.BACK);
  }
}

/** Macro to move the cursor to the start of the input field. */
export class NavStartText extends RepeatableKeyPressMacro {
  constructor(inputController: InputController) {
    super(
        MacroName.NAV_START_TEXT, 1,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
  }

  override doKeyPress(): void {
    // TODO(b/259397131): Migrate this implementation to use
    // chrome.automation.setDocumentSelection.
    EventGenerator.sendKeyPress(KeyCode.HOME, {ctrl: true});
  }
}

/** Macro to move the cursor to the end of the input field. */
export class NavEndText extends RepeatableKeyPressMacro {
  constructor(inputController: InputController) {
    super(
        MacroName.NAV_END_TEXT, 1,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
  }

  override doKeyPress(): void {
    // TODO(b/259397131): Migrate this implementation to use
    // chrome.automation.setDocumentSelection.
    EventGenerator.sendKeyPress(KeyCode.END, {ctrl: true});
  }
}

/** Macro to select the previous word in the input field. */
export class SelectPrevWord extends RepeatableKeyPressMacro {
  /** @param repeat The number of previous words to select. */
  constructor(inputController: InputController, repeat = 1) {
    super(
        MacroName.SELECT_PREV_WORD, repeat,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
  }

  override doKeyPress(): void {
    EventGenerator.sendKeyPress(KeyCode.LEFT, {ctrl: true, shift: true});
  }
}

/** Macro to select the next word in the input field. */
export class SelectNextWord extends RepeatableKeyPressMacro {
  /** @param repeat The number of next words to select. */
  constructor(inputController: InputController, repeat = 1) {
    super(
        MacroName.SELECT_NEXT_WORD, repeat,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
  }

  override doKeyPress(): void {
    EventGenerator.sendKeyPress(KeyCode.RIGHT, {ctrl: true, shift: true});
  }
}

/** Macro to select the next character in the input field. */
export class SelectNextChar extends RepeatableKeyPressMacro {
  /** @param repeat The number of next characters to select. */
  constructor(inputController: InputController, repeat = 1) {
    super(
        MacroName.SELECT_NEXT_CHAR, repeat,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
  }

  override doKeyPress(): void {
    EventGenerator.sendKeyPress(KeyCode.RIGHT, {shift: true});
  }
}

/** Macro to select the previous character in the input field. */
export class SelectPrevChar extends RepeatableKeyPressMacro {
  /** @param repeat The number of previous characters to select. */
  constructor(inputController: InputController, repeat = 1) {
    super(
        MacroName.SELECT_PREV_CHAR, repeat,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
  }

  override doKeyPress(): void {
    EventGenerator.sendKeyPress(KeyCode.LEFT, {shift: true});
  }
}

TestImportManager.exportForTesting(UnselectTextMacro);

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles braille input keys when the user is typing or editing
 * text in an input field.  This class cooperates with the Braille IME
 * that is built into Chrome OS to do the actual text editing.
 */

import {EventGenerator} from '/common/event_generator.js';
import {StringUtil} from '/common/string_util.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {BrailleKeyCommand, BrailleKeyEvent} from '../../common/braille/braille_key_types.js';
import {Spannable} from '../../common/spannable.js';

import {BrailleTranslatorManager} from './braille_translator_manager.js';
import {ExpandingBrailleTranslator} from './expanding_braille_translator.js';
import {LibLouis} from './liblouis.js';
import {ExtraCellsSpan, ValueSelectionSpan, ValueSpan} from './spans.js';

export class BrailleInputHandler {
  constructor() {
    /**
     * Port of the connected IME if any.
     * @private {Port}
     */
    this.imePort_ = null;
    /**
     * {code true} when the Braille IME is connected and has signaled that it is
     * active.
     * @private {boolean}
     */
    this.imeActive_ = false;
    /**
     * The input context of the current input field, as reported by the IME.
     * {@code null} if no input field has focus.
     * @private {?{contextID: number, type: string}}
     */
    this.inputContext_ = null;
    /**
     * Text that currently precedes the first selection end-point.
     * @private {string}
     */
    this.currentTextBefore_ = '';
    /**
     * Text that currently follows the last selection end-point.
     * @private {string}
     */
    this.currentTextAfter_ = '';
    /**
     * Cells that were entered while the IME wasn't active.  These will be
     * submitted once the IME becomes active and reports the current input
     * field. This is necessary because the IME is activated on the first
     * braille dots command, but we'll receive the command in parallel.  To work
     * around the race, we store the cell entered until we can submit it to the
     * IME.
     * @private {!Array<number>}
     */
    this.pendingCells_ = [];
    /** @private {BrailleInputHandler.EntryState_} */
    this.entryState_ = null;
    /** @private {ExtraCellsSpan} */
    this.uncommittedCellsSpan_ = null;
    /** @private {function()?} */
    this.uncommittedCellsChangedListener_ = null;

    this.initListeners_();
  }

  /**
   * Starts to listen for connections from the Chrome OS braille IME.
   * @private
   */
  initListeners_() {
    BrailleTranslatorManager.instance.addChangeListener(
        () => this.commitAndClearEntryState_());

    chrome.runtime.onConnectExternal.addListener(
        port => this.onImeConnect_(port));
  }

  static init() {
    if (BrailleInputHandler.instance) {
      throw new Error('Cannot create two BrailleInputHandler instances');
    }
    BrailleInputHandler.instance = new BrailleInputHandler();
  }

  /**
   * Called when the content on the braille display is updated.  Modifies the
   * input state according to the new content.
   * @param {Spannable} text Text, optionally with value and selection spans.
   * @param {function()} listener Called when the uncommitted cells
   *     have changed.
   */
  onDisplayContentChanged(text, listener) {
    const valueSpan = text.getSpanInstanceOf(ValueSpan);
    const selectionSpan = text.getSpanInstanceOf(ValueSelectionSpan);
    if (!(valueSpan && selectionSpan)) {
      return;
    }
    // Don't call the old listener any further, since new content is being
    // set.  If the old listener is not cleared here, it could be called
    // spuriously if the entry state is cleared below.
    this.uncommittedCellsChangedListener_ = null;
    const valueStart = text.getSpanStart(valueSpan);
    const valueEnd = text.getSpanEnd(valueSpan);
    const selectionStart = text.getSpanStart(selectionSpan);
    const selectionEnd = text.getSpanEnd(selectionSpan);
    if (selectionStart < valueStart || selectionEnd > valueEnd) {
      console.error('Selection outside of value in braille content');
      this.clearEntryState_();
      return;
    }
    const newTextBefore = text.toString().substring(valueStart, selectionStart);
    if (this.currentTextBefore_ !== newTextBefore && this.entryState_) {
      this.entryState_.onTextBeforeChanged(newTextBefore);
    }
    this.currentTextBefore_ = newTextBefore;
    this.currentTextAfter_ = text.toString().substring(selectionEnd, valueEnd);
    this.uncommittedCellsSpan_ = new ExtraCellsSpan();
    text.setSpan(this.uncommittedCellsSpan_, selectionStart, selectionStart);
    if (this.entryState_ && this.entryState_.usesUncommittedCells) {
      this.updateUncommittedCells_(
          new Uint8Array(this.entryState_.cells_).buffer);
    }
    this.uncommittedCellsChangedListener_ = listener;
  }

  /**
   * Handles braille key events used for input by editing the current input
   * field appropriately.
   * @param {!BrailleKeyEvent} event The key event.
   * @return {boolean} {@code true} if the event was handled, {@code false}
   *     if it should propagate further.
   */
  onBrailleKeyEvent(event) {
    if (event.command === BrailleKeyCommand.DOTS) {
      return this.onBrailleDots_(/** @type {number} */ (event.brailleDots));
    }
    // Any other braille command cancels the pending cells.
    this.pendingCells_.length = 0;
    if (event.command === BrailleKeyCommand.STANDARD_KEY) {
      if (event.standardKeyCode === 'Backspace' && !event.altKey &&
          !event.ctrlKey && !event.shiftKey && this.onBackspace_()) {
        return true;
      } else {
        this.commitAndClearEntryState_();
        this.sendKeyEventPair_(event);
        return true;
      }
    }
    return false;
  }

  /**
   * Returns how the value of the currently displayed content should be
   * expanded given the current input state.
   * @return {ExpandingBrailleTranslator.ExpansionType}
   *     The current expansion type.
   */
  getExpansionType() {
    if (this.inAlwaysUncontractedContext_()) {
      return ExpandingBrailleTranslator.ExpansionType.ALL;
    }
    if (this.entryState_ &&
        this.entryState_.translator ===
            BrailleTranslatorManager.instance.getDefaultTranslator()) {
      return ExpandingBrailleTranslator.ExpansionType.NONE;
    }
    return ExpandingBrailleTranslator.ExpansionType.SELECTION;
  }

  /**
   * @return {boolean} {@code true} if we have an input context and
   *     uncontracted braille should always be used for that context.
   * @private
   */
  inAlwaysUncontractedContext_() {
    const inputType = this.inputContext_ ? this.inputContext_.type : '';
    return inputType === 'url' || inputType === 'email';
  }

  /**
   * Called when a user typed a braille cell.
   * @param {number} dots The dot pattern of the cell.
   * @return {boolean} Whether the event was handled or should be allowed to
   *    propagate further.
   * @private
   */
  onBrailleDots_(dots) {
    if (!this.imeActive_) {
      this.pendingCells_.push(dots);
      return true;
    }
    if (!this.inputContext_) {
      return false;
    }
    if (!this.entryState_) {
      if (!(this.entryState_ = this.createEntryState_())) {
        return false;
      }
    }
    this.entryState_.appendCell(dots);
    return true;
  }

  /**
   * Handles the backspace key by deleting the last typed cell if possible.
   * @return {boolean} {@code true} if the event was handled, {@code false}
   *     if it wasn't and should propagate further.
   * @private
   */
  onBackspace_() {
    if (this.imeActive_ && this.entryState_) {
      this.entryState_.deleteLastCell();
      return true;
    }
    return false;
  }

  /**
   * Creates a new empty {@code EntryState_} based on the current input
   * context and surrounding text.
   * @return {BrailleInputHandler.EntryState_} The newly created state
   *     object, or null if it couldn't be created (e.g. if there's no braille
   *     translator available yet).
   * @private
   */
  createEntryState_() {
    let translator = BrailleTranslatorManager.instance.getDefaultTranslator();
    if (!translator) {
      return null;
    }
    const uncontractedTranslator =
        BrailleTranslatorManager.instance.getUncontractedTranslator();
    let constructor = BrailleInputHandler.EditsEntryState_;
    if (uncontractedTranslator) {
      const textBefore = this.currentTextBefore_;
      const textAfter = this.currentTextAfter_;
      if (this.inAlwaysUncontractedContext_() ||
          (BrailleInputHandler.ENDS_WITH_NON_WHITESPACE_RE_.test(textBefore)) ||
          (BrailleInputHandler.STARTS_WITH_NON_WHITESPACE_RE_.test(
              textAfter))) {
        translator = uncontractedTranslator;
      } else {
        constructor = BrailleInputHandler.LateCommitEntryState_;
      }
    }

    return new constructor(this, translator);
  }

  /**
   * Commits the current entry state and clears it, if any.
   * @private
   */
  commitAndClearEntryState_() {
    if (this.entryState_) {
      this.entryState_.commit();
      this.clearEntryState_();
    }
  }

  /**
   * Clears the current entry state without committing it.
   * @private
   */
  clearEntryState_() {
    if (this.entryState_) {
      if (this.entryState_.usesUncommittedCells) {
        this.updateUncommittedCells_(new ArrayBuffer(0));
      }
      this.entryState_.inputHandler_ = null;
      this.entryState_ = null;
    }
  }

  /**
   * @param {ArrayBuffer} cells
   * @private
   */
  updateUncommittedCells_(cells) {
    if (this.uncommittedCellsSpan_) {
      this.uncommittedCellsSpan_.cells = cells;
    }
    if (this.uncommittedCellsChangedListener_) {
      this.uncommittedCellsChangedListener_();
    }
  }

  /**
   * Called when another extension connects to this extension.  Accepts
   * connections from the ChromeOS builtin Braille IME and ignores connections
   * from other extensions.
   * @param {Port} port The port used to communicate with the other extension.
   * @private
   */
  onImeConnect_(port) {
    if (port.name !== BrailleInputHandler.IME_PORT_NAME_ ||
        port.sender.id !== BrailleInputHandler.IME_EXTENSION_ID_) {
      return;
    }
    if (this.imePort_) {
      this.imePort_.disconnect();
    }
    port.onDisconnect.addListener(() => this.onImeDisconnect_(port));
    port.onMessage.addListener(message => this.onImeMessage_(message));
    this.imePort_ = port;
  }

  /**
   * Called when a message is received from the IME.
   * @param {*} message The message.
   * @private
   */
  onImeMessage_(message) {
    if (!goog.isObject(message)) {
      console.error(
          'Unexpected message from Braille IME: ', JSON.stringify(message));
    }
    switch (message.type) {
      case 'activeState':
        this.imeActive_ = message.active;
        break;
      case 'inputContext':
        this.inputContext_ = message.context;
        this.clearEntryState_();
        if (this.imeActive_ && this.inputContext_) {
          this.pendingCells_.forEach(this.onBrailleDots_, this);
        }
        this.pendingCells_.length = 0;
        break;
      case 'brailleDots':
        this.onBrailleDots_(message['dots']);
        break;
      case 'backspace':
        // Note that we can't send the backspace key through the
        // virtualKeyboardPrivate API in this case because it would then be
        // processed by the IME again, leading to an infinite loop.
        this.postImeMessage_({
          type: 'keyEventHandled',
          requestId: message['requestId'],
          result: this.onBackspace_(),
        });
        break;
      case 'reset':
        this.clearEntryState_();
        break;
      default:
        console.error(
            'Unexpected message from Braille IME: ', JSON.stringify(message));
        break;
    }
  }

  /**
   * Called when the IME port is disconnected.
   * @param {Port} port The port that was disconnected.
   * @private
   */
  onImeDisconnect_(port) {
    this.imePort_ = null;
    this.clearEntryState_();
    this.imeActive_ = false;
    this.inputContext_ = null;
  }

  /**
   * Posts a message to the IME.
   * @param {Object} message The message.
   * @return {boolean} {@code true} if the message was sent, {@code false} if
   *     there was no connection open to the IME.
   * @private
   */
  postImeMessage_(message) {
    if (this.imePort_) {
      this.imePort_.postMessage(message);
      return true;
    }
    return false;
  }

  /**
   * Sends a {@code keydown} key event followed by a {@code keyup} event
   * corresponding to an event generated by the braille display.
   * @param {!BrailleKeyEvent} event The braille key event to base the
   *     key events on.
   * @private
   */
  sendKeyEventPair_(event) {
    const keyName = /** @type {string} */ (event.standardKeyCode);
    const numericCode = BrailleKeyEvent.keyCodeToLegacyCode(keyName);
    if (!numericCode) {
      throw Error('Unknown key code in event: ' + JSON.stringify(event));
    }
    EventGenerator.sendKeyPress(numericCode, {
      shift: Boolean(event.shiftKey),
      ctrl: Boolean(event.ctrlKey),
      alt: Boolean(event.altKey),
    });
  }
}

/**
 * The ID of the Braille IME extension built into Chrome OS.
 * @const {string}
 * @private
 */
BrailleInputHandler.IME_EXTENSION_ID_ = 'jddehjeebkoimngcbdkaahpobgicbffp';

/**
 * Name of the port to use for communicating with the Braille IME.
 * @const {string}
 * @private
 */
BrailleInputHandler.IME_PORT_NAME_ = 'BrailleIme.Port';

/**
 * Regular expression that matches a string that starts with at least one
 * non-whitespace character.
 * @const {RegExp}
 * @private
 */
BrailleInputHandler.STARTS_WITH_NON_WHITESPACE_RE_ = /^\S/;

/**
 * Regular expression that matches a string that ends with at least one
 * non-whitespace character.
 * @const {RegExp}
 * @private
 */
BrailleInputHandler.ENDS_WITH_NON_WHITESPACE_RE_ = /\S$/;


/**
 * The entry state is the state related to entering a series of braille cells
 * without 'interruption', where interruption can be things like non braille
 * keyboard input or unexpected changes to the text surrounding the cursor.
 * @private
 */
BrailleInputHandler.EntryState_ = class {
  /**
   * @param {!BrailleInputHandler} inputHandler
   * @param {!LibLouis.Translator} translator
   */
  constructor(inputHandler, translator) {
    /** @private {BrailleInputHandler} */
    this.inputHandler_ = inputHandler;
    /**
     * The translator currently used for typing, if
     * {@code this.cells_.length > 0}.
     * @private {!LibLouis.Translator}
     */
    this.translator_ = translator;
    /**
     * Braille cells that have been typed by the user so far.
     * @private {!Array<number>}
     */
    this.cells_ = [];
    /**
     * Text resulting from translating {@code this.cells_}.
     * @private {string}
     */
    this.text_ = '';
    /**
     * List of strings that we expect to be set as preceding text of the
     * selection. This is populated when we send text changes to the IME so
     * that our own changes don't reset the pending cells.
     * @private {!Array<string>}
     */
    this.pendingTextsBefore_ = [];
  }

  /**
   * @return {!LibLouis.Translator} The translator used by this entry
   *     state. This doesn't change for a given object.
   */
  get translator() {
    return this.translator_;
  }

  /**
   * Appends a braille cell to the current input and updates the text if
   * necessary.
   * @param {number} cell The braille cell to append.
   */
  appendCell(cell) {
    this.cells_.push(cell);
    this.updateText_();
  }

  /**
   * Deletes the last cell of the input and updates the text if neccary.
   * If there's no more input in this object afterwards, clears the entry state
   * of the input handler.
   */
  deleteLastCell() {
    if (--this.cells_.length <= 0) {
      this.sendTextChange_('');
      this.inputHandler_.clearEntryState_();
      return;
    }
    this.updateText_();
  }

  /**
   * Called when the text before the cursor changes giving this object a
   * chance to clear the entry state of the input handler if the change
   * wasn't expected.
   * @param {string} newText New text before the cursor.
   */
  onTextBeforeChanged(newText) {
    // See if we are expecting this change as a result of one of our own
    // edits. Allow changes to be coalesced by the input system in an attempt
    // to not be too brittle.
    for (let i = 0; i < this.pendingTextsBefore_.length; ++i) {
      if (newText === this.pendingTextsBefore_[i]) {
        // Delete all previous expected changes and ignore this one.
        this.pendingTextsBefore_.splice(0, i + 1);
        return;
      }
    }
    // There was an actual text change (or cursor movement) that we hadn't
    // caused ourselves, reset any pending input.
    this.inputHandler_.clearEntryState_();
  }

  /**
   * Makes sure the current text is permanently added to the edit field.
   * After this call, this object should be abandoned.
   */
  commit() {}

  /**
   * @return {boolean} true if the entry state uses uncommitted cells.
   */
  get usesUncommittedCells() {
    return false;
  }

  /**
   * Updates the translated text based on the current cells and sends the
   * delta to the IME.
   * @private
   */
  updateText_() {
    const cellsBuffer = new Uint8Array(this.cells_).buffer;
    const commit = this.lastCellIsBlank_;
    if (!commit && this.usesUncommittedCells) {
      this.inputHandler_.updateUncommittedCells_(cellsBuffer);
    }
    this.translator_.backTranslate(cellsBuffer, result => {
      if (result === null) {
        console.error('Error when backtranslating braille cells');
        return;
      }
      if (!this.inputHandler_) {
        return;
      }
      this.sendTextChange_(result);
      this.text_ = result;
      if (commit) {
        this.inputHandler_.commitAndClearEntryState_();
      }
    });
  }

  /**
   * @return {boolean}
   * @private
   */
  get lastCellIsBlank_() {
    return this.cells_[this.cells_.length - 1] === 0;
  }

  /**
   * Sends new text to the IME.  This dhould be overriden by subclasses.
   * The old text is still available in the {@code text_} property.
   * @param {string} newText Text to send.
   * @private
   */
  sendTextChange_(newText) {}
};


/**
 * Entry state that uses {@code deleteSurroundingText} and {@code commitText}
 * calls to the IME to update the currently enetered text.
 * @private
 */
BrailleInputHandler.EditsEntryState_ =
    class extends BrailleInputHandler.EntryState_ {
  /**
   * @param {!BrailleInputHandler} inputHandler
   * @param {!LibLouis.Translator} translator
   */
  constructor(inputHandler, translator) {
    super(inputHandler, translator);
  }

  /** @override */
  sendTextChange_(newText) {
    const oldText = this.text_;
    // Find the common prefix of the old and new text.
    const commonPrefixLength =
        StringUtil.longestCommonPrefixLength(oldText, newText);
    // How many characters we need to delete from the existing text to replace
    // them with characters from the new text.
    const deleteLength = oldText.length - commonPrefixLength;
    // New text, if any, to insert after deleting the deleteLength characters
    // before the cursor.
    const toInsert = newText.substring(commonPrefixLength);
    if (deleteLength > 0 || toInsert.length > 0) {
      // After deleting, we expect this text to be present before the cursor.
      const textBeforeAfterDelete =
          this.inputHandler_.currentTextBefore_.substring(
              0, this.inputHandler_.currentTextBefore_.length - deleteLength);
      if (deleteLength > 0) {
        // Queue this text up to be ignored when the change comes in.
        this.pendingTextsBefore_.push(textBeforeAfterDelete);
      }
      if (toInsert.length > 0) {
        // Likewise, queue up what we expect to be before the cursor after
        // the replacement text is inserted.
        this.pendingTextsBefore_.push(textBeforeAfterDelete + toInsert);
      }
      // Send the replace operation to be performed asynchronously by the IME.
      this.inputHandler_.postImeMessage_({
        type: 'replaceText',
        contextID: this.inputHandler_.inputContext_.contextID,
        deleteBefore: deleteLength,
        newText: toInsert,
      });
    }
  }
};


/**
 * Entry state that only updates the edit field when a blank cell is entered.
 * During the input of a single 'word', the uncommitted text is stored by the
 * IME.
 * @private
 */
BrailleInputHandler.LateCommitEntryState_ =
    class extends BrailleInputHandler.EntryState_ {
  /**
   * @param {!BrailleInputHandler} inputHandler
   * @param {!LibLouis.Translator} translator
   */
  constructor(inputHandler, translator) {
    super(inputHandler, translator);
  }

  /** @override */
  commit() {
    this.inputHandler_.postImeMessage_({
      type: 'commitUncommitted',
      contextID: this.inputHandler_.inputContext_.contextID,
    });
  }

  /** @override */
  get usesUncommittedCells() {
    return true;
  }

  /** @override */
  sendTextChange_(newText) {
    this.inputHandler_.postImeMessage_({
      type: 'setUncommitted',
      contextID: this.inputHandler_.inputContext_.contextID,
      text: newText,
    });
  }
};

/** @type {BrailleInputHandler} */
BrailleInputHandler.instance;

TestImportManager.exportForTesting(BrailleInputHandler);

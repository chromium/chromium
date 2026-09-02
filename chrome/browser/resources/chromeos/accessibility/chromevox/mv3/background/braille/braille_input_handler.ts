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
import {CandidateMenuBackground} from '../panel/candidate_menu_background.js';

import {BrailleTranslator} from './braille_translator.js';
import {BrailleTranslatorManager} from './braille_translator_manager.js';
import {CompositionCandidateProvider} from './composition_candidate_provider.js';
import {ExpandingBrailleTranslator} from './expanding_braille_translator.js';
import {ExtraCellsSpan, ValueSelectionSpan, ValueSpan} from './spans.js';

type Port = chrome.runtime.Port;

interface Context {
  contextID: number;
  type: string;
}

/**
 * Regular expression that matches a string that starts with at least one
 * non-whitespace character.
 */
const STARTS_WITH_NON_WHITESPACE_RE = /^\S/;

/**
 * Regular expression that matches a string that ends with at least one
 * non-whitespace character.
 */
const ENDS_WITH_NON_WHITESPACE_RE = /\S$/;

type EntryStateConstructor = new (
    handler: BrailleInputHandler, translator: BrailleTranslator) => EntryState;

/**
 * The entry state is the state related to entering a series of braille cells
 * without 'interruption', where interruption can be things like non braille
 * keyboard input or unexpected changes to the text surrounding the cursor.
 */
class EntryState {
  inputHandler: BrailleInputHandler|null;
  /** Braille cells that have been typed by the user so far. */
  cells: number[] = [];
  /** Text resulting from translating this.cells. */
  text = '';

  /**
   * List of strings that we expect to be set as preceding text of the
   * selection. This is populated when we send text changes to the IME so
   * that our own changes don't reset the pending cells.
   */
  protected pendingTextsBefore_: string[] = [];

  constructor(
      inputHandler: BrailleInputHandler,
      private translator_: BrailleTranslator) {
    this.inputHandler = inputHandler;
  }

  /**
   * @return The translator used by this entry state. This doesn't change for a
   * given object.
   */
  get translator(): BrailleTranslator {
    return this.translator_;
  }

  /**
   * Appends a braille cell to the current input and updates the text if
   * necessary.
   * @param cell The braille cell to append.
   */
  appendCell(cell: number): void {
    this.cells.push(cell);
    this.updateText_();
  }

  /**
   * Deletes the last cell of the input and updates the text if neccary.
   * If there's no more input in this object afterwards, clears the entry state
   * of the input handler.
   */
  deleteLastCell(): void {
    if (--this.cells.length <= 0) {
      this.sendTextChange_('');
      this.inputHandler?.clearEntryState();
      return;
    }
    this.updateText_();
  }

  /**
   * Called when the text before the cursor changes giving this object a
   * chance to clear the entry state of the input handler if the change
   * wasn't expected.
   * @param newText New text before the cursor.
   */
  onTextBeforeChanged(newText: string): void {
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
    this.inputHandler?.clearEntryState();
  }

  /**
   * Makes sure the current text is permanently added to the edit field.
   * After this call, this object should be abandoned.
   */
  commit(): void {}

  /**
   * Replaces the current entry text with `newText`, sending the appropriate
   * delta to the IME. Used by composition conversion to display conversion
   * candidates in place of the entered composition text.
   */
  setText(newText: string): void {
    this.sendTextChange_(newText);
    this.text = newText;
  }

  /**
   * @return true if the entry state uses uncommitted cells.
   */
  get usesUncommittedCells(): boolean {
    return false;
  }

  /**
   * Updates the translated text based on the current cells and sends the
   * delta to the IME.
   */
  private updateText_(): void {
    const cellsBuffer = new Uint8Array(this.cells).buffer;
    const commit = this.lastCellIsBlank_;
    if (!commit && this.usesUncommittedCells) {
      this.inputHandler?.updateUncommittedCells(cellsBuffer);
    }
    this.translator_.backTranslate(cellsBuffer, result => {
      if (result === null) {
        console.error('Error when backtranslating braille cells');
        return;
      }
      if (!this.inputHandler) {
        return;
      }
      this.sendTextChange_(result);
      this.text = result;
      if (commit) {
        this.inputHandler.requestCommit(this);
      }
    });
  }

  private get lastCellIsBlank_(): boolean {
    return this.cells[this.cells.length - 1] === 0;
  }

  /**
   * Sends new text to the IME.  This should be overridden by subclasses.
   * The old text is still available in the text property.
   */
  protected sendTextChange_(_newText: string): void {}
}

/**
 * Entry state that uses deleteSurroundingText and commitText calls to the IME
 * to update the currently entered text.
 */
class EditsEntryState extends EntryState {
  protected override sendTextChange_(newText: string): void {
    const oldText = this.text;
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
          this.inputHandler?.currentTextBefore.substring(
              0, this.inputHandler.currentTextBefore.length - deleteLength);
      if (deleteLength > 0 && textBeforeAfterDelete !== undefined) {
        // Queue this text up to be ignored when the change comes in. Note
        // that this includes the empty string, which occurs when all text
        // before the cursor is replaced (e.g. by composition conversion).
        this.pendingTextsBefore_.push(textBeforeAfterDelete);
      }
      if (toInsert.length > 0) {
        // Likewise, queue up what we expect to be before the cursor after
        // the replacement text is inserted.
        this.pendingTextsBefore_.push(textBeforeAfterDelete + toInsert);
      }
      // Send the replace operation to be performed asynchronously by the IME.
      this.inputHandler?.postImeMessage({
        type: 'replaceText',
        contextID: this.inputHandler.inputContext?.contextID,
        deleteBefore: deleteLength,
        newText: toInsert,
      });
    }
  }
}


/**
 * Entry state that only updates the edit field when a blank cell is entered.
 * During the input of a single 'word', the uncommitted text is stored by the
 * IME.
 */
class LateCommitEntryState extends EntryState {
  override commit(): void {
    this.inputHandler?.postImeMessage({
      type: 'commitUncommitted',
      contextID: this.inputHandler.inputContext?.contextID,
    });
  }

  override get usesUncommittedCells(): boolean {
    return true;
  }

  protected override sendTextChange_(newText: string): void {
    this.inputHandler?.postImeMessage({
      type: 'setUncommitted',
      contextID: this.inputHandler.inputContext?.contextID,
      text: newText,
    });
  }
}

/**
 * Entry state that enters text as IME composition (preedit) text instead of
 * committing it directly. The composition is rendered inline in the field
 * (typically underlined), giving a visual indication that the text is not
 * final. Used for languages that require composition conversion (currently
 * Japanese kana input, converted to kanji before being committed). Because
 * composition updates replace the whole preedit atomically in the IME, this
 * avoids the delete/insert churn of EditsEntryState during conversion.
 */
class CompositionEntryState extends EntryState {
  /**
   * Text that preceded the cursor when the composition started, or null if
   * no composition has been sent yet. The composition text appears in the
   * field value after this text.
   */
  private baseTextBefore_: string|null = null;

  override commit(): void {
    this.inputHandler?.postImeMessage({
      type: 'commitComposition',
      contextID: this.inputHandler.inputContext?.contextID,
    });
  }

  override onTextBeforeChanged(newText: string): void {
    // Composition text is part of the field value, so composition updates
    // change the text before the cursor. Tolerate any text that starts with
    // the base text observed when the composition started; asynchronous
    // echoes of stale composition states would otherwise reset the entry
    // state mid-word.
    if (this.baseTextBefore_ !== null &&
        newText.startsWith(this.baseTextBefore_)) {
      return;
    }
    this.inputHandler?.clearEntryState();
  }

  protected override sendTextChange_(newText: string): void {
    if (this.baseTextBefore_ === null) {
      this.baseTextBefore_ = this.inputHandler?.currentTextBefore ?? '';
    }
    this.inputHandler?.postImeMessage({
      type: 'setComposition',
      contextID: this.inputHandler.inputContext?.contextID,
      text: newText,
    });
  }
}

export class BrailleInputHandler {
  /** Port of the connected IME if any. */
  private imePort_: Port|null = null;
  /**
   * True when the Braille IME is connected and has signaled that it is
   * active.
   */
  private imeActive_ = false;
  /** Text that currently follows the last selection end-point. */
  private currentTextAfter_ = '';
  /**
   * Cells that were entered while the IME wasn't active.  These will be
   * submitted once the IME becomes active and reports the current input
   * field. This is necessary because the IME is activated on the first
   * braille dots command, but we'll receive the command in parallel.  To work
   * around the race, we store the cell entered until we can submit it to the
   * IME.
   */
  private pendingCells_: number[] = [];
  private entryState_: EntryState|null = null;
  private uncommittedCellsSpan_: ExtraCellsSpan|null = null;
  private uncommittedCellsChangedListener_: VoidFunction|null = null;
  /**
   * Whether a conversion request is in flight: fetching candidates, or the
   * user choosing one in the Panel's candidate menu.
   */
  private conversionPending_ = false;
  /**
   * Cells entered while a conversion request was pending (see
   * conversionPending_). Replayed through onBrailleDots_() once the request
   * settles, so cells typed in the meantime are applied to whatever state
   * results (a new word after a commit or cancellation) instead of being
   * appended to the entry state the fetch/menu was tracking.
   */
  private queuedCellsWhilePending_: number[] = [];
  /** Resolves when the most recent asynchronous commit request settles. */
  commitRequestForTest: Promise<void>|null = null;

  /**
   * The input context of the current input field, as reported by the IME.
   * null if no input field has focus.
   */
  inputContext: Context|null = null;

  /**
   * The first non-null input context reported while a conversion is
   * pending, i.e. the field settling back after the candidate menu's own
   * blur/refocus. Used instead of `inputContext` when replacing text on
   * accept, so a later, unexpected context change (the user genuinely
   * moving focus elsewhere while the menu is open, however unlikely with
   * the menu fullscreen) can't redirect the replacement to the wrong
   * field.
   */
  private pendingConversionContext_: Context|null = null;

  /** Text that currently precedes the first selection end-point. */
  currentTextBefore = '';

  static instance: BrailleInputHandler;

  /** The ID of the Braille IME extension built into Chrome OS. */
  static IME_EXTENSION_ID_ = 'jddehjeebkoimngcbdkaahpobgicbffp';
  /** Name of the port to use for communicating with the Braille IME. */
  static IME_PORT_NAME_ = 'BrailleIme.Port';

  constructor() {
    BrailleTranslatorManager.instance.addChangeListener(
        () => this.commitAndClearEntryState());

    chrome.runtime.onConnectExternal.addListener(
        (port: Port) => this.onImeConnect_(port));
  }

  static init(): void {
    if (BrailleInputHandler.instance) {
      throw new Error('Cannot create two BrailleInputHandler instances');
    }
    BrailleInputHandler.instance = new BrailleInputHandler();
  }

  /**
   * Called when the content on the braille display is updated.  Modifies the
   * input state according to the new content.
   * @param text Text, optionally with value and selection spans.
   * @param listener Called when the uncommitted cells have changed.
   */
  onDisplayContentChanged(text: Spannable, listener: VoidFunction): void {
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
      this.clearEntryState();
      return;
    }
    const newTextBefore = text.toString().substring(valueStart, selectionStart);
    if (this.currentTextBefore !== newTextBefore && this.entryState_) {
      this.entryState_.onTextBeforeChanged(newTextBefore);
    }
    this.currentTextBefore = newTextBefore;
    this.currentTextAfter_ = text.toString().substring(selectionEnd, valueEnd);
    this.uncommittedCellsSpan_ = new ExtraCellsSpan();
    text.setSpan(this.uncommittedCellsSpan_, selectionStart, selectionStart);
    if (this.entryState_ && this.entryState_.usesUncommittedCells) {
      this.updateUncommittedCells(
          new Uint8Array(this.entryState_.cells).buffer);
    }
    this.uncommittedCellsChangedListener_ = listener;
  }

  /**
   * Handles braille key events used for input by editing the current input
   * field appropriately.
   * @return true if the event was handled, false if it should propagate
   *     further.
   */
  onBrailleKeyEvent(event: BrailleKeyEvent): boolean {
    if (event.command === BrailleKeyCommand.DOTS) {
      return this.onBrailleDots_(event.brailleDots as number);
    }
    // Any other braille command cancels the pending cells.
    this.pendingCells_.length = 0;
    if (event.command === BrailleKeyCommand.STANDARD_KEY) {
      if (event.standardKeyCode === 'Backspace' && !event.altKey &&
          !event.ctrlKey && !event.shiftKey && this.onBackspace_()) {
        return true;
      }
      this.commitAndClearEntryState();
      this.sendKeyEventPair_(event);
      return true;
    }
    return false;
  }

  /**
   * Returns how the value of the currently displayed content should be
   * expanded given the current input state.
   */
  getExpansionType(): ExpandingBrailleTranslator.ExpansionType {
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
   * @return true if we have an input context and uncontracted braille should
   * always be used for that context.
   */
  private inAlwaysUncontractedContext_(): boolean {
    const inputType = this.inputContext ? this.inputContext.type : '';
    return inputType === 'url' || inputType === 'email';
  }

  /**
   * Called when a user typed a braille cell.
   * @param dots The dot pattern of the cell.
   * @return Whether the event was handled or should be allowed to
   *    propagate further.
   */
  private onBrailleDots_(dots: number): boolean {
    if (!this.imeActive_) {
      this.pendingCells_.push(dots);
      return true;
    }
    if (!this.inputContext) {
      return false;
    }
    if (this.conversionPending_) {
      // `conversionPending_` covers both fetching candidates and, if any
      // were found, the user choosing one in the Panel's candidate menu (see
      // maybeStartConversion_). Cells typed in either case are queued and
      // replayed once that settles.
      this.queuedCellsWhilePending_.push(dots);
      return true;
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
   * @return true if the event was handled, false if it wasn't and should
   * propagate further.
   */
  private onBackspace_(): boolean {
    if (this.imeActive_ && this.entryState_) {
      this.entryState_.deleteLastCell();
      return true;
    }
    return false;
  }

  /**
   * Creates a new empty EntryState based on the current input
   * context and surrounding text.
   * @return The newly created state object, or null if it couldn't be created
   *     (e.g. if there's no braille translator available yet).
   */
  private createEntryState_(): EntryState|null {
    let translator = BrailleTranslatorManager.instance.getDefaultTranslator();
    if (!translator) {
      return null;
    }
    // Translators for languages that require composition conversion enter
    // text as IME composition (preedit) text instead of committing it
    // directly.
    if (translator.usesCompositionInput) {
      return new CompositionEntryState(this, translator);
    }
    const uncontractedTranslator =
        BrailleTranslatorManager.instance.getUncontractedTranslator();
    let constructor: EntryStateConstructor = EditsEntryState;
    if (uncontractedTranslator) {
      const textBefore = this.currentTextBefore;
      const textAfter = this.currentTextAfter_;
      if (this.inAlwaysUncontractedContext_() ||
          (ENDS_WITH_NON_WHITESPACE_RE.test(textBefore)) ||
          (STARTS_WITH_NON_WHITESPACE_RE.test(textAfter))) {
        translator = uncontractedTranslator;
      } else {
        constructor = LateCommitEntryState;
      }
    }

    return new constructor(this, translator);
  }

  /**
   * Called by the entry state when a blank cell was entered, requesting that
   * the current input be committed. For languages that require composition
   * conversion (currently Japanese kana input), this may start conversion
   * instead of committing directly.
   */
  requestCommit(entryState: EntryState): void {
    if (entryState !== this.entryState_) {
      return;
    }
    const provider = entryState.translator.getCompositionCandidateProvider?.();
    if (!provider) {
      this.commitAndClearEntryState();
      return;
    }
    this.conversionPending_ = true;
    this.queuedCellsWhilePending_ = [];
    this.commitRequestForTest =
        this.maybeStartConversion_(entryState, entryState.text, provider);
  }

  /**
   * Fetches conversion candidates for `originalText` from `provider` and, if
   * any are available, enters conversion mode. Otherwise commits the input
   * as-is. Whether `originalText` is actually convertible is entirely up to
   * `provider`; an empty result is treated the same as "not convertible."
   */
  private async maybeStartConversion_(
      entryState: EntryState, originalText: string,
      provider: CompositionCandidateProvider): Promise<void> {
    const cellCount = entryState.cells.length;
    let candidates: string[] = [];
    try {
      // An empty result (including one left over from a failed fetch, since
      // `candidates` stays []) is treated as "not convertible" below.
      candidates = await provider.getCandidates(originalText);
    } catch (error) {
      console.error('Error getting conversion candidates: ' + error);
    }
    this.conversionPending_ = false;
    const queued = this.queuedCellsWhilePending_;
    this.queuedCellsWhilePending_ = [];
    // The entry state may have changed while fetching candidates (e.g. a
    // backspace deleted a cell directly, or focus moved elsewhere).
    if (this.entryState_ !== entryState ||
        entryState.cells.length !== cellCount ||
        entryState.text !== originalText) {
      return;
    }
    if (candidates.length === 0) {
      this.commitAndClearEntryState();
      for (const dots of queued) {
        this.onBrailleDots_(dots);
      }
      return;
    }
    // Commit the entered text as ordinary text before opening the menu,
    // instead of leaving it as an active IME composition for as long as the
    // menu is open. An uncommitted composition gets special, platform-level
    // treatment for Enter: it's committed immediately, before the key can
    // ever reach the Panel's own key handling, so selecting a candidate
    // with Enter would otherwise always commit the composition instead. A
    // selected candidate replaces this committed text via 'replaceText'; on
    // cancel, the text is already sitting there correctly, so there's
    // nothing further to do.
    this.commitAndClearEntryState();
    // Stay "pending" (queuing further cells, per onBrailleDots_ above) for
    // the whole time the user is choosing a candidate in the Panel's menu,
    // not just while candidates were being fetched, so cells queued during
    // either phase are replayed together once the menu resolves rather than
    // the fetch-phase ones being lost. This must come after
    // commitAndClearEntryState() above, since that clears both fields back
    // out via clearEntryState().
    this.conversionPending_ = true;
    this.queuedCellsWhilePending_ = queued;
    const selected = await CandidateMenuBackground.open(candidates);
    this.conversionPending_ = false;
    // Use the context that settled back in while the conversion was
    // pending, rather than one captured before the menu opened: opening
    // and closing the fullscreen Panel blurs and refocuses the underlying
    // field, which the IME framework treats as a brand new input context
    // (a new contextID), not a resumption of the old one. Sending
    // 'replaceText' with a stale ID fails silently on the IME side
    // ("Context is not active"), even though the message itself (selected
    // candidate, deleteBefore) was otherwise correct. Preferring the
    // locked-in pendingConversionContext_ over the live inputContext
    // guards against the (very unlikely, given the menu is fullscreen)
    // case where focus genuinely moved to a different field afterward.
    const contextID =
        (this.pendingConversionContext_ ?? this.inputContext)?.contextID;
    this.pendingConversionContext_ = null;
    if (selected !== null) {
      this.postImeMessage({
        type: 'replaceText',
        contextID,
        deleteBefore: originalText.length,
        newText: selected,
      });
    }
    const requeue = this.queuedCellsWhilePending_;
    this.queuedCellsWhilePending_ = [];
    for (const dots of requeue) {
      this.onBrailleDots_(dots);
    }
  }

  /** Commits the current entry state and clears it, if any. */
  commitAndClearEntryState(): void {
    if (this.entryState_) {
      this.entryState_.commit();
      this.clearEntryState();
    }
  }

  /** Clears the current entry state without committing it. */
  clearEntryState(): void {
    this.conversionPending_ = false;
    this.queuedCellsWhilePending_ = [];
    if (this.entryState_) {
      if (this.entryState_.usesUncommittedCells) {
        this.updateUncommittedCells(new ArrayBuffer(0));
      }
      this.entryState_.inputHandler = null;
      this.entryState_ = null;
    }
  }

  updateUncommittedCells(cells: ArrayBuffer): void {
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
   * @param port The port used to communicate with the other extension.
   */
  private onImeConnect_(port: Port): void {
    if (port.name !== BrailleInputHandler.IME_PORT_NAME_ ||
        port.sender!.id !== BrailleInputHandler.IME_EXTENSION_ID_) {
      return;
    }
    if (this.imePort_) {
      this.imePort_.disconnect();
    }
    port.onDisconnect.addListener(() => this.onImeDisconnect_(port));
    port.onMessage.addListener((message: any) => this.onImeMessage_(message));
    this.imePort_ = port;
  }

  /** Called when a message is received from the IME. */
  private onImeMessage_(message: any): void {
    if (typeof message !== 'object') {
      console.error(
          'Unexpected message from Braille IME: ', JSON.stringify(message));
    }
    switch (message.type) {
      case 'activeState':
        this.imeActive_ = message.active;
        break;
      case 'inputContext':
        this.inputContext = message.context;
        // Opening (and closing) the fullscreen candidate menu blurs and
        // refocuses the underlying field, which the IME framework reports
        // as a brand new input context, same as it does for a genuine
        // focus change elsewhere. While a conversion is pending, this is
        // always that spurious blur/refocus rather than a real context
        // switch, since the fullscreen menu leaves nothing else to focus;
        // clearing the entry state here would otherwise cancel the
        // in-progress conversion out from under the open menu.
        if (!this.conversionPending_) {
          this.clearEntryState();
        } else if (!this.pendingConversionContext_ && this.inputContext) {
          // Lock onto the first settled (non-null) context reported while
          // pending, rather than whatever's most recently reported by the
          // time the conversion resolves: see pendingConversionContext_.
          this.pendingConversionContext_ = this.inputContext;
        }
        if (this.imeActive_ && this.inputContext) {
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
        this.postImeMessage({
          type: 'keyEventHandled',
          requestId: message['requestId'],
          result: this.onBackspace_(),
        });
        break;
      case 'reset':
        // Same spurious blur/refocus rationale as 'inputContext' above: the
        // fullscreen candidate menu opening/closing can make the IME send
        // this too, and it shouldn't cancel an in-progress conversion.
        if (!this.conversionPending_) {
          this.clearEntryState();
        }
        break;
      default:
        console.error(
            'Unexpected message from Braille IME: ', JSON.stringify(message));
        break;
    }
  }

  /**
   * Called when the IME port is disconnected.
   * @param port The port that was disconnected.
   */
  private onImeDisconnect_(_port: Port): void {
    this.imePort_ = null;
    // The IME reconnects its port periodically even with no user action;
    // same spurious-churn rationale as 'inputContext'/'reset' above, this
    // shouldn't cancel an in-progress conversion.
    if (!this.conversionPending_) {
      this.clearEntryState();
    }
    this.imeActive_ = false;
    this.inputContext = null;
  }

  /**
   * Posts a message to the IME.
   * @param message The message.
   * @return true if the message was sent, false if there was no connection
   * open to the IME.
   */
  postImeMessage(message: Object): boolean {
    if (this.imePort_) {
      this.imePort_.postMessage(message);
      return true;
    }
    return false;
  }

  /**
   * Sends a keydown key event followed by a keyup event corresponding to an
   * event generated by the braille display.
   * @param event The braille key event to base the key events on.
   */
  private sendKeyEventPair_(event: BrailleKeyEvent): void {
    // TODO(crbug.com/314203187): Not null asserted, check that this is correct.
    const keyName = event.standardKeyCode!;
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

TestImportManager.exportForTesting(BrailleInputHandler);

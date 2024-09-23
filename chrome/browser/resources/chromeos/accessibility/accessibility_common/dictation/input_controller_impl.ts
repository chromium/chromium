// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EditableNodeData, InputController} from '/common/action_fulfillment/input_controller.js';
import {EventHandler} from '/common/event_handler.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {EditingUtil} from './editing_util.js';
import {FocusHandler} from './focus_handler.js';
import {LocaleInfo} from './locale_info.js';

type AutomationNode = chrome.automation.AutomationNode;
import EventType = chrome.automation.EventType;
import PositionType = chrome.automation.PositionType;
import StateType = chrome.automation.StateType;

interface SurroundingInfo {
  anchor: number;
  focus: number;
  offset: number;
  text: string;
}

/** A helper class that waits for automation and IME events. */
class AutomationImeEventWaiter {
  private node_: AutomationNode;
  private event_: EventType;

  constructor(node: AutomationNode, event: EventType) {
    this.node_ = node;
    this.event_ = event;
  }

  /**
   * Calls |doAction|, then waits for |this.event_| and a
   * chrome.input.ime.onSurroundingTextChanged event. We need to wait for both
   * since we use the automation and IME APIs to retrieve the editable node
   * data.
   */
  async doActionAndWait(doAction: () => void): Promise<void> {
    let surroundingTextChanged = false;
    let eventSeen = false;
    return new Promise(resolve => {
      const onSurroundingTextChanged: () => void = () => {
        surroundingTextChanged = true;
        chrome.input.ime.onSurroundingTextChanged.removeListener(
            onSurroundingTextChanged);
        if (eventSeen) {
          resolve();
        }
      };

      let handler: EventHandler|null =
          new EventHandler([this.node_], this.event_, () => {
            eventSeen = true;
            // TODO(b/314203187): Determine if not null assertion is acceptable.
            handler!.stop();
            handler = null;
            if (surroundingTextChanged) {
              resolve();
            }
          });

      handler.start();
      chrome.input.ime.onSurroundingTextChanged.addListener(
          onSurroundingTextChanged);

      doAction();
    });
  }
}

/** InputController handles interaction with input fields for Dictation. */
// TODO(b/307904022): Remove dependency on chrome.input.ime and depend fully on
// Automation instead, then
// TODO(b/324316493): Refactor logic that isn't specific to Dictation into
// common InputController.
export class InputControllerImpl extends InputController {
  private stopDictationCallback_: () => void;
  private focusHandler_: FocusHandler;
  private activeImeContextId_: number =
      InputControllerImpl.NO_ACTIVE_IME_CONTEXT_ID_;
  private onConnectCallback_: (() => void)|null = null;
  private onFocusListener_:
      ((context: chrome.input.ime.InputContext) => void)|null = null;
  private onBlurListener_: ((contextId: number) => void)|null = null;
  /** A listener for chrome.input.ime.onSurroundingTextChanged events. */
  private onSurroundingTextChangedListener_:
      ((engineID: string,
        surroundingInfo: SurroundingInfo) => void)|null = null;
  private surroundingInfo_: SurroundingInfo|null = null;
  private onSurroundingTextChangedForTesting_: (() => void)|null = null;
  private onSelectionChangedForTesting_: (() => void)|null = null;
  /**
   * The engine ID of the previously active IME input method. Used to
   * restore the previous IME after Dictation is deactivated.
   */
  private previousImeEngineId_ = '';
  constructor(stopDictationCallback: () => void, focusHandler: FocusHandler) {
    super();
    this.stopDictationCallback_ = stopDictationCallback;
    this.focusHandler_ = focusHandler;

    this.initialize_();
  }

  /** Sets up Dictation's various IME listeners. */
  private initialize_(): void {
    this.onFocusListener_ = context => this.onImeFocus_(context);
    this.onBlurListener_ = contextId => this.onImeBlur_(contextId);
    this.onSurroundingTextChangedListener_ = (engineID, surroundingInfo) =>
        this.onSurroundingTextChanged_(engineID, surroundingInfo);
    chrome.input.ime.onFocus.addListener(this.onFocusListener_);
    chrome.input.ime.onBlur.addListener(this.onBlurListener_);
    chrome.input.ime.onSurroundingTextChanged.addListener(
        this.onSurroundingTextChangedListener_);
  }

  /** Removes IME listeners. */
  removeListeners(): void {
    if (this.onFocusListener_) {
      chrome.input.ime.onFocus.removeListener(this.onFocusListener_);
    }
    if (this.onBlurListener_) {
      chrome.input.ime.onBlur.removeListener(this.onBlurListener_);
    }
    if (this.onSurroundingTextChangedListener_) {
      chrome.input.ime.onSurroundingTextChanged.removeListener(
          this.onSurroundingTextChangedListener_);
    }
  }

  /** Whether this is the active IME and has a focused input. */
  isActive(): boolean {
    return this.activeImeContextId_ !==
        InputControllerImpl.NO_ACTIVE_IME_CONTEXT_ID_;
  }

  /**
   * Connect as the active Input Method Manager.
   * @param callback The callback to run after IME is connected.
   */
  connect(callback: () => void): void {
    this.onConnectCallback_ = callback;
    chrome.inputMethodPrivate.getCurrentInputMethod(
        (method: string) => this.saveCurrentInputMethodAndStart_(method));
  }

  /**
   * Called when InputController has received the current input method. We save
   * the current method to reset when InputController toggles off, then continue
   * with starting up Dictation after the input gets focus (onImeFocus_()).
   * @param method The currently active IME ID.
   */
  private saveCurrentInputMethodAndStart_(method: string): void {
    this.previousImeEngineId_ = method;
    // Add AccessibilityCommon as an input method and activate it.
    chrome.languageSettingsPrivate.addInputMethod(
        InputControllerImpl.IME_ENGINE_ID);
    chrome.inputMethodPrivate.setCurrentInputMethod(
        InputControllerImpl.IME_ENGINE_ID);
  }

  /**
   * Disconnects as the active Input Method Manager. If any text was being
   * composed, commits it.
   */
  disconnect(): void {
    // Clean up IME state and reset to the previous IME method.
    this.activeImeContextId_ = InputControllerImpl.NO_ACTIVE_IME_CONTEXT_ID_;
    chrome.inputMethodPrivate.setCurrentInputMethod(this.previousImeEngineId_);
    this.previousImeEngineId_ = '';
    this.surroundingInfo_ = null;
  }

  /**
   * Commits the given text to the active IME context.
   * @param text The text to commit
   */
  commitText(text: string): void {
    if (!this.isActive() || !text) {
      return;
    }

    const data = this.getEditableNodeData();
    if (LocaleInfo.allowSmartCapAndSpacing() &&
        this.checkEditableNodeData_(data)) {
      const {value, selStart} = data as EditableNodeData;
      text = EditingUtil.smartCapitalization(value, selStart, text);
      text = EditingUtil.smartSpacing(value, selStart, text);
    }

    chrome.input.ime.commitText({contextID: this.activeImeContextId_, text});
  }

  /**
   * chrome.input.ime.onFocus callback. Save the active context ID, and
   * finish starting speech recognition if needed. This needs to be done
   * before starting recognition in order for browser tests to know that
   * Dictation is already set up as an IME.
   * @param context Input field context.
   */
  private onImeFocus_(context: chrome.input.ime.InputContext): void {
    this.activeImeContextId_ = context.contextID;
    if (this.onConnectCallback_) {
      this.onConnectCallback_();
      this.onConnectCallback_ = null;
    }
  }

  /**
   * chrome.input.ime.onFocus callback. Stops Dictation if the active
   * context ID lost focus.
   */
  private onImeBlur_(contextId: number): void {
    if (contextId === this.activeImeContextId_) {
      // Clean up context ID immediately. We can no longer use this context.
      this.activeImeContextId_ = InputControllerImpl.NO_ACTIVE_IME_CONTEXT_ID_;
      this.surroundingInfo_ = null;
      this.stopDictationCallback_();
    }
  }

  /**
   * Called when the editable string around the caret is changed or when the
   * caret position is moved.
   */
  private onSurroundingTextChanged_(
      engineID: string, surroundingInfo: SurroundingInfo): void {
    if (engineID !==
        InputControllerImpl.ON_SURROUNDING_TEXT_CHANGED_ENGINE_ID) {
      return;
    }

    this.surroundingInfo_ = surroundingInfo;
    if (this.onSurroundingTextChangedForTesting_) {
      this.onSurroundingTextChangedForTesting_();
    }
  }

  /**
   * Deletes the sentence to the left of the text caret. If the caret is in the
   * middle of a sentence, it will delete a portion of the sentence it
   * intersects.
   */
  deletePrevSentence(): void {
    const data = this.getEditableNodeData();
    if (!this.checkEditableNodeData_(data)) {
      return;
    }

    const {value, selStart} = data as EditableNodeData;
    const prevSentenceStart = EditingUtil.navPrevSent(value, selStart);
    const length = selStart - prevSentenceStart;
    this.deleteSurroundingText_(length, -length);
  }

  /**
   * @param length The number of characters to be deleted.
   * @param offset The offset from the caret position where deletion will start.
   *     This value can be negative.
   */
  private async deleteSurroundingText_(length: number, offset: number):
      Promise<void> {
    const editableNode = this.focusHandler_.getEditableNode();
    if (!editableNode) {
      throw new Error('deleteSurroundingText_ requires a valid editable node');
    }

    const deleteSurroundingText: () => void = () => {
      chrome.input.ime.deleteSurroundingText({
        contextID: this.activeImeContextId_,
        engineID: InputControllerImpl.IME_ENGINE_ID,
        length,
        offset,
      });
    };

    // Delete the surrounding text and wait for events to propagate.
    const waiter = new AutomationImeEventWaiter(
        editableNode, EventType.VALUE_IN_TEXT_FIELD_CHANGED);
    await waiter.doActionAndWait(deleteSurroundingText);
  }

  /**
   * Deletes a phrase to the left of the text caret. If multiple instances of
   * `phrase` are present, it deletes the one closest to the text caret.
   * @param phrase The phrase to be deleted.
   */
  deletePhrase(phrase: string): void {
    this.replacePhrase(phrase, '');
  }

  /**
   * Replaces a phrase to the left of the text caret with another phrase. If
   * multiple instances of `deletePhrase` are present, this function will
   * replace the one closest to the text caret.
   * @param deletePhrase The phrase to be deleted.
   * @param insertPhrase The phrase to be inserted.
   */
  async replacePhrase(deletePhrase: string, insertPhrase: string):
      Promise<void> {
    const data = this.getEditableNodeData();
    if (!this.checkEditableNodeData_(data)) {
      return;
    }

    const {value, selStart} = data as EditableNodeData;
    const replacePhraseData =
        EditingUtil.getReplacePhraseData(value, selStart, deletePhrase);
    if (!replacePhraseData) {
      return;
    }

    const {newIndex, deleteLength} = replacePhraseData;
    await this.setSelection_(newIndex, newIndex);
    await this.deleteSurroundingText_(deleteLength, deleteLength);
    if (insertPhrase) {
      this.commitText(insertPhrase);
    }
  }

  /**
   * Sets the selection within the editable node. `selStart` and `selEnd` are
   * relative to the value of the editable node. Works in all types of text
   * fields, including content editables.
   */
  private async setSelection_(selStart: number, selEnd: number): Promise<void> {
    const editableNode = this.focusHandler_.getEditableNode();
    if (!editableNode) {
      return;
    }

    let anchorObject = editableNode;
    let anchorOffset = selStart;
    let focusObject = editableNode;
    let focusOffset = selEnd;

    // TODO(b/314203187): Determine if not null assertion is acceptable.
    const isContentEditable = editableNode.state![StateType.RICHLY_EDITABLE];
    if (isContentEditable) {
      // Contenteditables can contain multiple inline text nodes, so we need to
      // translate `selStart` and `selEnd` to a node and index within the
      // contenteditable.
      let data = this.textNodeAndIndex_(selStart);
      if (data) {
        anchorObject = data.node;
        anchorOffset = data.index;
      }
      data = this.textNodeAndIndex_(selEnd);
      if (data) {
        focusObject = data.node;
        focusOffset = data.index;
      }
    }

    const setDocumentSelection: () => void = () => {
      chrome.automation.setDocumentSelection(
          {anchorObject, anchorOffset, focusObject, focusOffset});
    };

    // Set selection and wait for events to propagate.
    const waiter = new AutomationImeEventWaiter(
        editableNode, EventType.TEXT_SELECTION_CHANGED);
    await waiter.doActionAndWait(setDocumentSelection);
    if (this.onSelectionChangedForTesting_) {
      this.onSelectionChangedForTesting_();
    }
  }

  /**
   * Inserts `insertPhrase` directly before `beforePhrase` (and separates them
   * with a space). This function operates on the text to the left of the caret.
   * If multiple instances of `beforePhrase` are present, this function will
   * use the one closest to the text caret.
   */
  async insertBefore(insertPhrase: string, beforePhrase: string):
      Promise<void> {
    const data = this.getEditableNodeData();
    if (!this.checkEditableNodeData_(data)) {
      return;
    }

    const {value, selStart} = data as EditableNodeData;
    const newIndex =
        EditingUtil.getInsertBeforeIndex(value, selStart, beforePhrase);
    if (newIndex === -1) {
      return;
    }

    await this.setSelection_(newIndex, newIndex);
    this.commitText(insertPhrase);
  }

  /**
   * Sets selection starting at `startPhrase` and ending at `endPhrase`
   * (inclusive). The function operates on the text to the left of the text
   * caret. If multiple instances of `startPhrase` or `endPhrase` are present,
   * the function will use the ones closest to the text caret.
   */
  selectBetween(startPhrase: string, endPhrase: string): void {
    const data = this.getEditableNodeData();
    if (!this.checkEditableNodeData_(data)) {
      return;
    }

    const {value, selStart} = data as EditableNodeData;
    const selection =
        EditingUtil.selectBetween(value, selStart, startPhrase, endPhrase);
    if (!selection) {
      return;
    }

    this.setSelection_(selection.start, selection.end);
  }

  /** Moves the text caret to the next sentence. */
  async navNextSent(): Promise<void> {
    const data = this.getEditableNodeData();
    if (!this.checkEditableNodeData_(data)) {
      return;
    }

    const {value, selStart} = data as EditableNodeData;
    const newCaretIndex = EditingUtil.navNextSent(value, selStart);
    await this.setSelection_(newCaretIndex, newCaretIndex);
  }

  /** Moves the text caret to the previous sentence. */
  async navPrevSent(): Promise<void> {
    const data = this.getEditableNodeData();
    if (!this.checkEditableNodeData_(data)) {
      return;
    }

    const {value, selStart} = data as EditableNodeData;
    const newCaretIndex = EditingUtil.navPrevSent(value, selStart);
    await this.setSelection_(newCaretIndex, newCaretIndex);
  }

  /**
   * Returns the editable node, its value, the selection start, and the
   * selection end.
   * TODO(crbug.com/1353871): Only return text that is visible on-screen.
   */
  getEditableNodeData(): EditableNodeData|null {
    const node = this.focusHandler_.getEditableNode();
    if (!node) {
      return null;
    }

    let value;
    let selStart;
    let selEnd;
    // TODO(b/314203187): Determine if not null assertion is acceptable.
    const isContentEditable = node.state![StateType.RICHLY_EDITABLE];
    if (isContentEditable && this.surroundingInfo_) {
      const info = this.surroundingInfo_;
      // Use IME data only in contenteditables.
      value = info.text;
      selStart = Math.min(info.anchor, info.focus);
      selEnd = Math.max(info.anchor, info.focus);
      return {node, value, selStart, selEnd};
    }

    // Fall back to data from Automation.
    value = node.value || '';
    selStart = (node.textSelStart !== undefined && node.textSelStart !== -1) ?
        node.textSelStart :
        value.length;
    selEnd = (node.textSelEnd !== undefined && node.textSelEnd !== -1) ?
        node.textSelEnd :
        value.length;
    return {
      node,
      value,
      selStart: Math.min(selStart, selEnd),
      selEnd: Math.max(selStart, selEnd),
    };
  }

  /**
   * Returns whether or not `data` meets the prerequisites for performing an
   * editing command.
   */
  private checkEditableNodeData_(data: EditableNodeData|null): boolean {
    if (!data || data.selStart !== data.selEnd) {
      // TODO(b:259353226): Move this selection check into checkContext()
      // method.
      return false;
    }

    return true;
  }

  /**
   * Translates `index`, which is relative to the editable's value, to an inline
   * text node and index within the editable. Only returns valid data when the
   * editable node is a contenteditable.
   */
  private textNodeAndIndex_(index: number):
      {node: AutomationNode, index: number}|null {
    const editableNode = this.focusHandler_.getEditableNode();
    // TODO(b/314203187): Determine if not null assertion is acceptable.
    if (!editableNode || !editableNode.state![StateType.RICHLY_EDITABLE]) {
      throw new Error('textNodeAndIndex_ requires a content editable node');
    }

    const position = editableNode.createPosition(PositionType.TEXT, index);
    position.asLeafTextPosition();
    if (!position || !position.node || position.textOffset === undefined) {
      return null;
    }

    return {
      node: position.node,
      index: position.textOffset,
    };
  }
}

export namespace InputControllerImpl {
  /** The IME engine ID for AccessibilityCommon. */
  export const IME_ENGINE_ID =
      '_ext_ime_egfdjlfmgnehecnclamagfafdccgfndpdictation';

  /**
   * The engine ID that is passed into `onSurroundingTextChanged_` when
   * Dictation modifies the text field.
   */
  export const ON_SURROUNDING_TEXT_CHANGED_ENGINE_ID = 'dictation';

  export const NO_ACTIVE_IME_CONTEXT_ID_ = -1;
}

TestImportManager.exportForTesting(InputControllerImpl);

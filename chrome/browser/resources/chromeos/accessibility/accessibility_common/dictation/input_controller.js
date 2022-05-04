// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const AutomationNode = chrome.automation.AutomationNode;
const AutomationEvent = chrome.automation.AutomationEvent;
const EventType = chrome.automation.EventType;
const IconType = chrome.accessibilityPrivate.DictationBubbleIconType;

/**
 * InputController handles interaction with input fields for Dictation.
 */
export class InputController {
  constructor(stopDictationCallback) {
    /** @private {number} */
    this.activeImeContextId_ = InputController.NO_ACTIVE_IME_CONTEXT_ID_;

    /**
     * The engine ID of the previously active IME input method. Used to
     * restore the previous IME after Dictation is deactivated.
     * @private {string}
     */
    this.previousImeEngineId_ = '';

    /** @private {function():void} */
    this.stopDictationCallback_ = stopDictationCallback;

    /** @private {?function():void} */
    this.onConnectCallback_ = null;

    /**
     * The currently focused editable node.
     * @private {?AutomationNode}
     */
    this.editableNode_ = null;

    /** @private {?EventHandler} */
    this.focusHandler_ = null;

    this.initialize_();
  }

  /**
   * Sets up Dictation's various IME listeners.
   * @private
   */
  initialize_() {
    // Listen for IME focus changes.
    chrome.input.ime.onFocus.addListener(
        (context) => this.onImeFocus_(context));
    chrome.input.ime.onBlur.addListener(
        (contextId) => this.onImeBlur_(contextId));

    // IME focus and blur listeners do not tell us which AutomationNode is
    // currently focused. Register a focus event handler that will give us this
    // information.
    this.focusHandler_ = new EventHandler(
        [], EventType.FOCUS, event => this.onFocusChanged_(event));
    chrome.automation.getDesktop((desktop) => {
      this.focusHandler_.setNodes(desktop);
      this.focusHandler_.start();
    });
  }

  /**
   * Whether this is the active IME and has a focused input.
   * @return {boolean}
   */
  isActive() {
    return this.activeImeContextId_ !==
        InputController.NO_ACTIVE_IME_CONTEXT_ID_;
  }

  /**
   * Connect as the active Input Method Manager.
   * @param {function():void} callback The callback to run after IME is
   *     connected.
   */
  connect(callback) {
    this.onConnectCallback_ = callback;
    chrome.inputMethodPrivate.getCurrentInputMethod(
        (method) => this.saveCurrentInputMethodAndStart_(method));
  }

  /**
   * Called when InputController has received the current input method. We save
   * the current method to reset when InputController toggles off, then continue
   * with starting up Dictation after the input gets focus (onImeFocus_()).
   * @param {string} method The currently active IME ID.
   * @private
   */
  saveCurrentInputMethodAndStart_(method) {
    this.previousImeEngineId_ = method;
    // Add AccessibilityCommon as an input method and activate it.
    chrome.languageSettingsPrivate.addInputMethod(
        InputController.IME_ENGINE_ID);
    chrome.inputMethodPrivate.setCurrentInputMethod(
        InputController.IME_ENGINE_ID);
  }

  /**
   * Disconnects as the active Input Method Manager. If any text was being
   * composed, commits it.
   */
  disconnect() {
    // Clean up IME state and reset to the previous IME method.
    this.activeImeContextId_ = InputController.NO_ACTIVE_IME_CONTEXT_ID_;
    chrome.inputMethodPrivate.setCurrentInputMethod(this.previousImeEngineId_);
    this.previousImeEngineId_ = '';
  }

  /**
   * Commits the given text to the active IME context.
   * @param {string} text The text to commit
   */
  commitText(text) {
    if (!this.isActive() || !text) {
      return;
    }

    text = this.adjustCommitText_(text);
    chrome.input.ime.commitText({contextID: this.activeImeContextId_, text});
  }

  /**
   * chrome.input.ime.onFocus callback. Save the active context ID, and
   * finish starting speech recognition if needed. This needs to be done
   * before starting recognition in order for browser tests to know that
   * Dictation is already set up as an IME.
   * @param {chrome.input.ime.InputContext} context Input field context.
   * @private
   */
  onImeFocus_(context) {
    this.activeImeContextId_ = context.contextID;
    if (this.onConnectCallback_) {
      this.onConnectCallback_();
      this.onConnectCallback_ = null;
    }
  }

  /**
   * chrome.input.ime.onFocus callback. Stops Dictation if the active
   * context ID lost focus.
   * @param {number} contextId
   * @private
   */
  onImeBlur_(contextId) {
    if (contextId === this.activeImeContextId_) {
      // Clean up context ID immediately. We can no longer use this context.
      this.activeImeContextId_ = InputController.NO_ACTIVE_IME_CONTEXT_ID_;
      this.stopDictationCallback_();
    }
  }

  /**
   * @param {!AutomationEvent} event
   * @private
   */
  onFocusChanged_(event) {
    const node = event.target;
    if (!node || !AutomationPredicate.editText(node)) {
      this.editableNode_ = null;
      return;
    }

    this.editableNode_ = node;
  }

  /**
   * @param {string} text
   * @return {string}
   */
  adjustCommitText_(text) {
    // There is currently a bug in SODA (b/213934503) where final speech results
    // do not start with a space. This results in a Dictation bug
    // (crbug.com/1294050), where final speech results are not separated by a
    // space when committed to a text field. This is a temporary workaround
    // until the blocking SODA bug can be fixed. Note, a similar strategy
    // already exists in Dictation::OnSpeechResult().
    if (!this.editableNode_ ||
        InputController.BEGINS_WITH_WHITESPACE_REGEX_.test(text)) {
      return text;
    }

    const value = this.editableNode_.value;
    const selStart = this.editableNode_.textSelStart;
    const selEnd = this.editableNode_.textSelEnd;
    // Prepend a space to `text` if there is text directly left of the cursor.
    if (!selStart || selStart !== selEnd || !value ||
        InputController.BEGINS_WITH_WHITESPACE_REGEX_.test(
            value[selStart - 1])) {
      return text;
    }

    return ' ' + text;
  }
}

/**
 * The IME engine ID for AccessibilityCommon.
 * @const {string}
 */
InputController.IME_ENGINE_ID =
    '_ext_ime_egfdjlfmgnehecnclamagfafdccgfndpdictation';

/**
 * @private {number}
 * @const
 */
InputController.NO_ACTIVE_IME_CONTEXT_ID_ = -1;

/**
 * @private {!RegExp}
 * @const
 */
InputController.BEGINS_WITH_WHITESPACE_REGEX_ = /^\s/;

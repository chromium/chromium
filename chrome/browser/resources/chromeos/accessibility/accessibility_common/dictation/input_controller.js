// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

    /**
     * The current composition text, if any.
     * @private {string}
     */
    this.currentComposition_ = '';

    /** @private {function():void} */
    this.stopDictationCallback_ = stopDictationCallback;

    /** @private {?function():void} */
    this.onConnectCallback_ = null;

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
    // Add AccessibilityCommon as an input method and active it.
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
    // Commit composition text, if any.
    if (this.currentComposition_.length > 0) {
      this.commitText(this.currentComposition_);
    }

    // Clean up IME state and reset to the previous IME method.
    this.activeImeContextId_ = InputController.NO_ACTIVE_IME_CONTEXT_ID_;
    chrome.inputMethodPrivate.setCurrentInputMethod(this.previousImeEngineId_);
    this.previousImeEngineId_ = '';
  }

  /**
   * @return {boolean} Whether any text is currently being composed.
   */
  hasCompositionText() {
    return this.currentComposition_.length > 0;
  }

  /** Displays current composition text for the current IME context. */
  displayCurrentComposition() {
    if (!this.isActive()) {
      return;
    }

    // Set the composition text for interim results.
    // Later we will do this in Chrome OS UI so that if the
    // result will become a command it will not appear and
    // disappear from the composition text.
    chrome.input.ime.setComposition({
      contextID: this.activeImeContextId_,
      cursor: this.currentComposition_.length,
      text: this.currentComposition_
    });
  }

  /**
   * Commits the given text to the active IME context.
   * @param {string} text The text to commit
   */
  commitText(text) {
    if (!this.isActive()) {
      return;
    }
    chrome.input.ime.commitText({contextID: this.activeImeContextId_, text});
    this.setCurrentComposition('');
  }

  /**
   * Shows the bubble UI with the given text.
   * @param {string} text
   */
  showBubble(text) {
    chrome.accessibilityPrivate.updateDictationBubble(/*visible=*/ true, text);
  }

  /** Hides the bubble UI. */
  hideBubble() {
    chrome.accessibilityPrivate.updateDictationBubble(/*visible=*/ false);
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

  /** @param {string} text */
  setCurrentComposition(text) {
    this.currentComposition_ = text;
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

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const IconType = chrome.accessibilityPrivate.DictationBubbleIconType;

/**
 * InputController handles interaction with input fields for Dictation.
 */
export class InputController {
  constructor(stopDictationCallback, focusHandler) {
    /** @private {number} */
    this.activeImeContextId_ = InputController.NO_ACTIVE_IME_CONTEXT_ID_;

    /** @private {!FocusHandler} */
    this.focusHandler_ = focusHandler;

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

    this.initialize_();
  }

  /**
   * Sets up Dictation's various IME listeners.
   * @private
   */
  initialize_() {
    // Listen for IME focus changes.
    chrome.input.ime.onFocus.addListener(context => this.onImeFocus_(context));
    chrome.input.ime.onBlur.addListener(
        contextId => this.onImeBlur_(contextId));
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
        method => this.saveCurrentInputMethodAndStart_(method));
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
    const editableNode = this.focusHandler_.getEditableNode();
    if (!editableNode ||
        InputController.BEGINS_WITH_WHITESPACE_REGEX_.test(text)) {
      return text;
    }

    const value = editableNode.value;
    const selStart = editableNode.textSelStart;
    const selEnd = editableNode.textSelEnd;
    // Prepend a space to `text` if there is text directly left of the cursor.
    if (!selStart || selStart !== selEnd || !value ||
        InputController.BEGINS_WITH_WHITESPACE_REGEX_.test(
            value[selStart - 1])) {
      return text;
    }

    return ' ' + text;
  }

  /**
   * Deletes the sentence to the left of the text caret. If the caret is in the
   * middle of a sentence, it will delete a portion of the sentence it
   * intersects.
   */
  deletePrevSentence() {
    const editableNode = this.focusHandler_.getEditableNode();
    if (!editableNode || !editableNode.value ||
        editableNode.textSelStart !== editableNode.textSelEnd) {
      return;
    }

    const value = editableNode.value;
    const caretIndex = editableNode.textSelStart;
    const prevSentenceStart =
        this.findPrevSentenceStartIndex_(value, caretIndex);
    const length = caretIndex - prevSentenceStart;
    this.deleteSurroundingText_(length, -length);
  }

  /**
   * Returns the start index of the sentence to the left of the caret. Indices
   * are relative to `text`. Assumes that sentences are separated by punctuation
   * specified in `InputController.END_OF_SENTENCE_REGEX_`.
   * @param {string} text
   * @param {number} caretIndex The index of the text caret.
   */
  findPrevSentenceStartIndex_(text, caretIndex) {
    let encounteredText = false;
    if (caretIndex === text.length) {
      --caretIndex;
    }

    while (caretIndex >= 0) {
      const valueAtCaret = text[caretIndex];
      if (encounteredText &&
          InputController.END_OF_SENTENCE_REGEX_.test(valueAtCaret)) {
        // Adjust if there is another sentence after this one.
        return text[caretIndex + 1] === ' ' ? caretIndex + 2 : caretIndex;
      }

      if (!InputController.BEGINS_WITH_WHITESPACE_REGEX_.test(valueAtCaret) &&
          !InputController.PUNCTUATION_REGEX_.test(valueAtCaret)) {
        encounteredText = true;
      }
      --caretIndex;
    }

    return 0;
  }

  /**
   * @param {number} length The number of characters to be deleted.
   * @param {number} offset The offset from the caret position where deletion
   * will start. This value can be negative.
   * @private
   */
  deleteSurroundingText_(length, offset) {
    chrome.input.ime.deleteSurroundingText({
      contextID: this.activeImeContextId_,
      engineID: InputController.IME_ENGINE_ID,
      length,
      offset
    });
  }

  /**
   * Deletes a phrase to the left of the text caret. If multiple instances of
   * `phrase` are present, it deletes the one closest to the text caret.
   * @param {string} phrase The phrase to be deleted.
   */
  smartDeletePhrase(phrase) {
    this.smartReplacePhrase(phrase, '');
  }

  /**
   * Replaces a phrase to the left of the text caret with another phrase. If
   * multiple instances of `deletePhrase` are present, this function will
   * replace the one closest to the text caret.
   * @param {string} deletePhrase The phrase to be deleted.
   * @param {string} insertPhrase The phrase to be inserted.
   */
  smartReplacePhrase(deletePhrase, insertPhrase) {
    const editableNode = this.focusHandler_.getEditableNode();
    if (!editableNode || !editableNode.value ||
        editableNode.textSelStart !== editableNode.textSelEnd) {
      return;
    }

    const value = editableNode.value;
    const caretIndex = editableNode.textSelStart;
    const leftOfCaret = value.substring(0, caretIndex);
    const rightOfCaret = value.substring(caretIndex);
    const performingDelete = insertPhrase === '';
    deletePhrase = deletePhrase.trim();
    insertPhrase = insertPhrase.trim();

    // Find the right-most occurrence of `deletePhrase`. Require `deletePhrase`
    // to be separated by word boundaries. If we're deleting text, prefer
    // the RegExps that include a leading/trailing space to preserve spacing.
    const re = new RegExp(`(\\b${deletePhrase}\\b)(?!.*\\b\\1\\b)`, 'i');
    const reWithLeadingSpace =
        new RegExp(`(\\b ${deletePhrase}\\b)(?!.*\\b\\1\\b)`, 'i');
    const reWithTrailingSpace =
        new RegExp(`(\\b${deletePhrase} \\b)(?!.*\\b\\1\\b)`, 'i');

    let newLeft;
    if (performingDelete && reWithLeadingSpace.test(leftOfCaret)) {
      newLeft = leftOfCaret.replace(reWithLeadingSpace, insertPhrase);
    } else if (performingDelete && reWithTrailingSpace.test(leftOfCaret)) {
      newLeft = leftOfCaret.replace(reWithTrailingSpace, insertPhrase);
    } else {
      newLeft = leftOfCaret.replace(re, insertPhrase);
    }

    const newValue = newLeft + rightOfCaret;
    editableNode.setValue(newValue);
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

/**
 * @private {!RegExp}
 * @const
 */
InputController.PUNCTUATION_REGEX_ =
    /[-$#"()*;:<>\n\\\/\{\}\[\]+='~`!@_.,?%\u2022\u25e6\u25a0]/g;

/**
 * @private {!RegExp}
 * @const
 */
InputController.END_OF_SENTENCE_REGEX_ = /[;!.?]/g;

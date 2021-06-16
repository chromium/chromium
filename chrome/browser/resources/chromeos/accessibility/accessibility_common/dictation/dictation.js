// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Dictation states.
 * @enum {!number}
 */
const DictationState = {
  OFF: 1,
  STARTING: 2,
  LISTENING: 3,
  STOPPING: 4,
};

/**
 * The IME engine ID for AccessibilityCommon.
 * @private {string}
 * @const
 */
const IME_ENGINE_ID = '_ext_ime_egfdjlfmgnehecnclamagfafdccgfndpdictation';

/**
 * Main class for the Chrome OS dictation feature.
 * Please note: this is being developed behind the flag
 * --enable-experimental-accessibility-dictation-extension
 */
export class Dictation {
  constructor() {
    chrome.accessibilityPrivate.onToggleDictation.addListener(
        this.onToggleDictation_.bind(this));
    chrome.input.ime.onFocus.addListener(this.onImeFocus_.bind(this));
    chrome.input.ime.onBlur.addListener(this.onImeBlur_.bind(this));

    /** @private {number} */
    this.activeImeContextId_ = -1;

    /**
     * The engine ID of the previously active IME input method. Used to
     * restore the previous IME after Dictation is deactivated.
     * @private {string}
     */
    this.previousImeEngineId_ = '';

    /**
     * The state of Dictation.
     * @private {!DictationState}
     */
    this.state_ = DictationState.OFF;
  }

  /**
   * Called when Dictation is toggled.
   * @param {boolean} activated Whether Dictation was just activated.
   * @private
   */
  onToggleDictation_(activated) {
    if (activated && this.state_ === DictationState.OFF) {
      this.state_ = DictationState.STARTING;
      chrome.inputMethodPrivate.getCurrentInputMethod((method) => {
        if (this.state_ !== DictationState.STARTING) {
          return;
        }
        this.previousImeEngineId_ = method;
        // Add AccessibilityCommon as an input method and active it.
        chrome.languageSettingsPrivate.addInputMethod(IME_ENGINE_ID);
        chrome.inputMethodPrivate.setCurrentInputMethod(IME_ENGINE_ID, () => {
          if (this.state_ === DictationState.STARTING) {
            // TODO(crbug.com/1216111): Start speech recognition and
            // change state to LISTENING after SR starts.
          } else {
            // We are no longer starting up - perhaps a stop came
            // through during the async callbacks. Ensure cleanup
            // by calling onDictationStopped_.
            this.onDictationStopped_();
          }
        });
      });
    } else {
      this.onDictationStopped_();
    }
  }

  /**
   * Stops Dictation in the browser / ash if it wasn't already stopped.
   * @private
   */
  stopDictation_() {
    // Stop Dictation if the state isn't already off.
    if (this.state_ !== DictationState.OFF) {
      chrome.accessibilityPrivate.toggleDictation();
      this.state_ = DictationState.STOPPING;
    }
  }

  /**
   * Called when Dictation has been toggled off. Cleans up IME and local state.
   * @private
   */
  onDictationStopped_() {
    if (this.state_ === DictationState.OFF) {
      return;
    }
    this.state_ = DictationState.OFF;
    // Clean up IME state and reset to the previous IME method.
    this.activeImeContextId_ = -1;
    chrome.inputMethodPrivate.setCurrentInputMethod(this.previousImeEngineId_);
    this.previousImeEngineId_ = '';
    Dictation.removeAsInputMethod();
  }

  /**
   * chrome.input.ime.onFocus callback. Save the active context ID.
   * @param {chrome.input.ime.InputContext} context Input field context.
   * @private
   */
  onImeFocus_(context) {
    this.activeImeContextId_ = context.contextID;
  }

  /**
   * chrome.input.ime.onFocus callback. Stops Dictation if the active
   * context ID lost focus.
   * @param {number} contextId
   * @private
   */
  onImeBlur_(contextId) {
    if (contextId === this.activeImeContextId_) {
      this.stopDictation_();
    }
  }

  /**
   * Removes AccessibilityCommon as an input method so it doesn't show up in
   * the shelf input method picker UI.
   */
  static removeAsInputMethod() {
    chrome.languageSettingsPrivate.removeInputMethod(IME_ENGINE_ID);
  }
}

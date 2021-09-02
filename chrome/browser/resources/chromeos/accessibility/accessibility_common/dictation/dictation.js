// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Main class for the Chrome OS dictation feature.
 * Please note: this is being developed behind the flag
 * --enable-experimental-accessibility-dictation-extension
 */
// TODO(crbug.com/1216111): Metrics and offline speech recognition
// functionality.
export class Dictation {
  constructor() {
    /** @private {number} */
    this.activeImeContextId_ = Dictation.NO_ACTIVE_IME_CONTEXT_ID_;

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

    /**
     * The state of Dictation.
     * @private {!Dictation.DictationState}
     */
    this.state_ = Dictation.DictationState.OFF;

    // TODO(crbug.com/1198212): Use SpeechRecognitionPrivate when available.
    /** @private {SpeechRecognition} */
    this.speechRecognizer_ = new window.webkitSpeechRecognition();

    /** @private {Audio} */
    this.cancelTone_ = new Audio('dictation/earcons/null_selection.wav');

    /** @private {Audio} */
    this.startTone_ = new Audio('dictation/earcons/audio_initiate.wav');

    /** @private {Audio} */
    this.endTone_ = new Audio('dictation/earcons/audio_end.wav');

    /** @private {?number} */
    this.timeoutId_ = null;

    this.initialize_();
  }

  /**
   * Sets up Dictation's speech recognizer and various listeners.
   * @private
   */
  initialize_() {
    this.speechRecognizer_.interimResults = true;
    this.speechRecognizer_.continuous = true;
    this.speechRecognizer_.onresult = event => this.onResult_(event);
    this.speechRecognizer_.onstart = () => this.onSpeechRecognitionStart_();
    this.speechRecognizer_.onend = () => this.onSpeechRecognitionEnd_();
    this.speechRecognizer_.onerror = error =>
        this.onSpeechRecognitionError_(error);

    chrome.settingsPrivate.getAllPrefs(prefs => this.updateFromPrefs_(prefs));
    chrome.settingsPrivate.onPrefsChanged.addListener(
        prefs => this.updateFromPrefs_(prefs));

    // Listen for IME focus changes.
    chrome.input.ime.onFocus.addListener(context => this.onImeFocus_(context));
    chrome.input.ime.onBlur.addListener(
        contextId => this.onImeBlur_(contextId));

    // Listen for Dictation toggles (activated / deactivated) from the Ash
    // Browser process.
    chrome.accessibilityPrivate.onToggleDictation.addListener(
        activated => this.onToggleDictation_(activated));
  }

  /**
   * Called when Dictation is toggled.
   * @param {boolean} activated Whether Dictation was just activated.
   * @private
   */
  onToggleDictation_(activated) {
    if (activated && this.state_ === Dictation.DictationState.OFF) {
      this.state_ = Dictation.DictationState.STARTING;
      this.startTone_.play();
      chrome.inputMethodPrivate.getCurrentInputMethod(
          method => this.maybeSaveCurrentInputMethodAndStart_(method));
    } else {
      this.onDictationStopped_();
    }
  }

  /**
   * Called when Dictation has received the current input method. We save the
   * current method to reset when Dictation toggles up, then continue with
   * starting up Dictation. Because this is async, checks that startup state
   * is still correct before proceeding to the next step.
   * @param {string} method
   * @private
   */
  maybeSaveCurrentInputMethodAndStart_(method) {
    if (this.state_ !== Dictation.DictationState.STARTING) {
      return;
    }
    this.previousImeEngineId_ = method;
    // Add AccessibilityCommon as an input method and active it.
    chrome.languageSettingsPrivate.addInputMethod(Dictation.IME_ENGINE_ID);
    chrome.inputMethodPrivate.setCurrentInputMethod(
        Dictation.IME_ENGINE_ID, () => this.maybeStartSpeechRecognition_());
  }

  /**
   * Called when Dictation has set itself as the IME during start-up. Because
   * this is async, checks that startup state is still correct before starting
   * speech recognition.
   * @private
   */
  maybeStartSpeechRecognition_() {
    if (this.state_ === Dictation.DictationState.STARTING) {
      this.speechRecognizer_.start();
      this.timeoutId_ = setTimeout(
          () => this.stopDictation_(), Dictation.SpeechTimeouts.NO_SPEECH_MS);
    } else {
      // We are no longer starting up - perhaps a stop came
      // through during the async callbacks. Ensure cleanup
      // by calling onDictationStopped_.
      this.onDictationStopped_();
    }
  }

  /**
   * Stops Dictation in the browser / ash if it wasn't already stopped.
   * The Dictation extension should always use this method to stop Dictation
   * to ensure that Browser/Ash knows that Dictation has stopped. When
   * AccessibilityManager receives the toggleDictation signal it will call
   * back through onDictationStopped_() for state cleanup.
   * @private
   */
  stopDictation_() {
    // Stop Dictation if the state isn't already off or turning off.
    if (this.state_ !== Dictation.DictationState.OFF &&
        this.state_ !== Dictation.DictationState.STOPPING) {
      chrome.accessibilityPrivate.toggleDictation();
      this.state_ = Dictation.DictationState.STOPPING;
    }
  }

  /**
   * Called when Dictation has been toggled off. Cleans up IME and local state.
   * @private
   */
  onDictationStopped_() {
    if (this.state_ === Dictation.DictationState.OFF) {
      return;
    }
    this.state_ = Dictation.DictationState.OFF;
    // Commit composition text, if any.
    if (this.currentComposition_.length > 0) {
      this.processSpeechRecognitionResult_(
          this.currentComposition_, /*isFinal=*/ true);
      this.endTone_.play();
    } else {
      this.cancelTone_.play();
    }
    // Stop speech recognition without sending a final result.
    this.speechRecognizer_.abort();

    // Clear any timeouts.
    if (this.timeoutId_ !== null) {
      clearTimeout(this.timeoutId_);
      this.timeoutId_ = null;
    }

    // Clean up IME state and reset to the previous IME method.
    this.activeImeContextId_ = Dictation.NO_ACTIVE_IME_CONTEXT_ID_;
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
      // Clean up context ID immediately. We can no longer use this context.
      this.activeImeContextId_ = Dictation.NO_ACTIVE_IME_CONTEXT_ID_;
      this.stopDictation_();
    }
  }

  /**
   * Called when the Speech Recognition engine receives a recognition event.
   * @param {SpeechRecognitionEvent} event
   * @private
   */
  onResult_(event) {
    if (this.state_ !== Dictation.DictationState.LISTENING) {
      return;
    }
    const result = event.results[event.resultIndex];
    if (result.length < 1) {
      return;
    }
    this.processSpeechRecognitionResult_(result[0].transcript, result.isFinal);
    clearTimeout(this.timeoutId_);
    this.timeoutId_ = setTimeout(
        () => this.stopDictation_(),
        result.isFinal ? Dictation.SpeechTimeouts.NO_SPEECH_MS :
                         Dictation.SpeechTimeouts.NO_NEW_SPEECH_MS);
  }

  /**
   * Processes a speech recognition result.
   * @param {string} transcript
   * @param {boolean} isFinal Whether this is a finalized transcript or an
   *     interim result.
   * @private
   */
  processSpeechRecognitionResult_(transcript, isFinal) {
    if (this.activeImeContextId_ === Dictation.NO_ACTIVE_IME_CONTEXT_ID_) {
      return;
    }
    if (isFinal) {
      chrome.input.ime.commitText(
          {contextID: this.activeImeContextId_, text: transcript});
      this.currentComposition_ = '';
    } else if (this.speechRecognizer_.interimResults) {
      // Set the composition text for interim results.
      // Later we will do this in a bubble so that if the
      // result will become a command it will not appear and
      // disappear from the composition text.
      chrome.input.ime.setComposition({
        contextID: this.activeImeContextId_,
        cursor: transcript.length,
        text: transcript
      });
      this.currentComposition_ = transcript;
    }
  }

  /**
   * Called when Speech Recognition starts up and begins listening.
   * @private
   */
  onSpeechRecognitionStart_() {
    if (this.state_ !== Dictation.DictationState.STARTING) {
      // We tried to stop during speech shutdown.
      return;
    }
    this.state_ = Dictation.DictationState.LISTENING;
  }

  /**
   * Called when Speech Recognition has ended, either because of the
   * SpeechRecognizer ending recognition or because recognition was cancelled.
   * @private
   */
  onSpeechRecognitionEnd_() {
    // Stop dictation if it wasn't already stopped.
    this.stopDictation_();
  }

  /**
   * @param {SpeechRecognitionError} error
   * @private
   */
  onSpeechRecognitionError_(error) {
    // TODO: Dictation does not surface speech recognition errors to the user.
    // Informing the user of errors, for example lack of network connection or a
    // missing microphone, would be a useful feature.
    this.stopDictation_();
  }

  /**
   * @param {!Array<!chrome.settingsPrivate.PrefObject>} prefs
   * @private
   */
  updateFromPrefs_(prefs) {
    prefs.forEach((pref) => {
      switch (pref.key) {
        case Dictation.DICTATION_LOCALE_PREF:
          if (pref.value) {
            this.speechRecognizer_.lang = /** @type {string} */ (pref.value);
          }
          break;
        case Dictation.SPOKEN_FEEDBACK_PREF:
          // When Spoken Feedback is enabled, we shouldn't populate interim
          // results because it will increase the verbosity too much.
          if (pref.value) {
            this.speechRecognizer_.interimResults = false;
          } else {
            this.speechRecognizer_.interimResults = true;
          }
          break;
        default:
          return;
      }
    });
  }

  /**
   * Removes AccessibilityCommon as an input method so it doesn't show up in
   * the shelf input method picker UI.
   */
  static removeAsInputMethod() {
    chrome.languageSettingsPrivate.removeInputMethod(Dictation.IME_ENGINE_ID);
  }
}

/**
 * Dictation states.
 * @enum {!number}
 */
Dictation.DictationState = {
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
Dictation.IME_ENGINE_ID = '_ext_ime_egfdjlfmgnehecnclamagfafdccgfndpdictation';

/**
 * @private {number}
 * @const
 */
Dictation.NO_ACTIVE_IME_CONTEXT_ID_ = -1;

/**
 * Dictation locale pref.
 * @type {string}
 * @const
 */
Dictation.DICTATION_LOCALE_PREF = 'settings.a11y.dictation_locale';

/**
 * ChromeVox enabled pref.
 * @type {string}
 * @const
 */
Dictation.SPOKEN_FEEDBACK_PREF = 'settings.accessibility';

/**
 * Timeout durations.
 * @type {!Object<string, number>}
 */
Dictation.SpeechTimeouts = {
  NO_SPEECH_MS: 10 * 1000,
  NO_NEW_SPEECH_MS: 5 * 1000,
};

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InputController} from '/accessibility_common/dictation/input_controller.js';
import {Macro} from '/accessibility_common/dictation/macros/macro.js';
import {MacroName} from '/accessibility_common/dictation/macros/macro_names.js';
import {MetricsUtils} from '/accessibility_common/dictation/metrics_utils.js';
import {SpeechParser} from '/accessibility_common/dictation/parse/speech_parser.js';
import {HintContext, UIController, UIState} from '/accessibility_common/dictation/ui_controller.js';

const ErrorEvent = chrome.speechRecognitionPrivate.SpeechRecognitionErrorEvent;
const ResultEvent =
    chrome.speechRecognitionPrivate.SpeechRecognitionResultEvent;
const StartOptions = chrome.speechRecognitionPrivate.StartOptions;
const StopEvent = chrome.speechRecognitionPrivate.SpeechRecognitionStopEvent;
const SpeechRecognitionType =
    chrome.speechRecognitionPrivate.SpeechRecognitionType;

/** Main class for the Chrome OS dictation feature. */
export class Dictation {
  constructor() {
    /** @private {InputController} */
    this.inputController_ = null;

    /** @private {UIController} */
    this.uiController_ = null;

    /** @private {SpeechParser} */
    this.speechParser_ = null;

    /** @private {string} */
    this.localePref_ = '';

    /**
     * Whether or not Dictation is active.
     * @private {boolean}
     */
    this.active_ = false;

    /** @private {Audio} */
    this.cancelTone_ = new Audio('dictation/earcons/null_selection.wav');

    /** @private {Audio} */
    this.startTone_ = new Audio('dictation/earcons/audio_initiate.wav');

    /** @private {Audio} */
    this.endTone_ = new Audio('dictation/earcons/audio_end.wav');

    /** @private {?number} */
    this.stopTimeoutId_ = null;

    /** @private {string} */
    this.interimText_ = '';

    /** @private {boolean} */
    this.chromeVoxEnabled_ = false;

    /** @private {?StartOptions} */
    this.speechRecognitionOptions_ = null;

    /** @private {?MetricsUtils} */
    this.metricsUtils_ = null;

    this.initialize_();
  }

  /**
   * Sets up Dictation's speech recognizer and various listeners.
   * @private
   */
  initialize_() {
    this.inputController_ =
        new InputController(() => this.stopDictation_(/*notify=*/ true));
    this.uiController_ = new UIController();
    this.speechParser_ = new SpeechParser(this.inputController_);
    if (this.localePref_) {
      this.speechParser_.initialize(this.localePref_);
    }

    // Set default speech recognition properties. Locale will be updated when
    // `updateFromPrefs_` is called.
    this.speechRecognitionOptions_ = {
      locale: 'en-US',
      interimResults: true,
    };

    // Setup speechRecognitionPrivate API listeners.
    chrome.speechRecognitionPrivate.onStop.addListener(
        event => this.onSpeechRecognitionStopped_(event));
    chrome.speechRecognitionPrivate.onResult.addListener(
        event => this.onSpeechRecognitionResult_(event));
    chrome.speechRecognitionPrivate.onError.addListener(
        event => this.onSpeechRecognitionError_(event));

    chrome.settingsPrivate.getAllPrefs(prefs => this.updateFromPrefs_(prefs));
    chrome.settingsPrivate.onPrefsChanged.addListener(
        prefs => this.updateFromPrefs_(prefs));

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
    if (activated && !this.active_) {
      this.startDictation_();
    } else {
      this.stopDictation_(/*notify=*/ false);
    }
  }

  /** @private */
  startDictation_() {
    this.active_ = true;
    this.startTone_.play();
    this.setStopTimeout_(Dictation.Timeouts.NO_FOCUSED_IME_MS);
    this.inputController_.connect(() => this.maybeStartSpeechRecognition_());
  }

  /**
   * Called when Dictation has set itself as the IME during start-up. Because
   * this is async, checks that startup state is still correct before starting
   * speech recognition.
   * @private
   */
  maybeStartSpeechRecognition_() {
    if (this.active_) {
      chrome.speechRecognitionPrivate.start(
          /** @type {!StartOptions} */ (this.speechRecognitionOptions_),
          (type) => this.onSpeechRecognitionStarted_(type));
      this.setStopTimeout_(Dictation.Timeouts.NO_SPEECH_MS);
    } else {
      // We are no longer starting up - perhaps a stop came
      // through during the async callbacks. Ensure cleanup
      // by calling stopDictation_().
      this.stopDictation_(/*notify=*/ true);
    }
  }

  /**
   * Stops Dictation and notifies the browser.
   * @param {boolean} notify True if we should notify the browser that Dictation
   * stopped.
   * @private
   */
  stopDictation_(notify) {
    if (!this.active_) {
      return;
    }

    this.active_ = false;
    // Stop speech recognition.
    chrome.speechRecognitionPrivate.stop({}, () => {});
    if (this.interimText_) {
      this.endTone_.play();
    } else {
      this.cancelTone_.play();
    }

    // Clear any timeouts.
    this.clearTimeoutIds_();

    this.inputController_.commitText(this.interimText_);
    this.hideCommandsUI_();
    this.inputController_.disconnect();
    Dictation.removeAsInputMethod();

    // Notify the browser that Dictation turned off.
    if (notify) {
      chrome.accessibilityPrivate.toggleDictation();
    }
  }

  /**
   * Sets the timeout to stop Dictation.
   * @param {number} durationMs
   * @private
   */
  setStopTimeout_(durationMs) {
    if (this.stopTimeoutId_ !== null) {
      clearTimeout(this.stopTimeoutId_);
    }
    this.stopTimeoutId_ =
        setTimeout(() => this.stopDictation_(/*notify=*/ true), durationMs);
  }

  /**
   * Called when the Speech Recognition engine receives a recognition event.
   * @param {ResultEvent} event
   * @private
   */
  async onSpeechRecognitionResult_(event) {
    if (!this.active_) {
      return;
    }

    const transcript = event.transcript;
    const isFinal = event.isFinal;
    this.setStopTimeout_(
        isFinal ? Dictation.Timeouts.NO_SPEECH_MS :
                  Dictation.Timeouts.NO_NEW_SPEECH_MS);
    await this.processSpeechRecognitionResult_(transcript, isFinal);
  }

  /**
   * Processes a speech recognition result.
   * @param {string} transcript
   * @param {boolean} isFinal Whether this is a finalized transcript or an
   *     interim result.
   * @private
   */
  async processSpeechRecognitionResult_(transcript, isFinal) {
    if (!isFinal) {
      this.showInterimText_(transcript);
      return;
    }

    const macro = await this.speechParser_.parse(transcript);
    MetricsUtils.recordMacroRecognized(macro);
    // Check if the macro can execute.
    // TODO(crbug.com/1264544): Deal with ambiguous results here.
    const checkContextResult = macro.checkContext();
    if (!checkContextResult.canTryAction) {
      this.showMacroExecutionFailed_(macro, transcript);
      return;
    }

    // Try to run the macro.
    const runMacroResult = macro.runMacro();
    if (!runMacroResult.isSuccess) {
      this.showMacroExecutionFailed_(macro, transcript);
      return;
    }
    if (macro.getMacroName() === MacroName.LIST_COMMANDS) {
      // ListCommandsMacro opens a new tab, thereby changing the cursor focus
      // and ending the Dictation session.
      return;
    }

    // Provide feedback to the user that the macro executed successfully.
    this.showMacroExecuted_(macro, transcript);
  }

  /**
   * Called when Speech Recognition starts up and begins listening. Passed as
   * a callback to speechRecognitionPrivate.start().
   * @param {SpeechRecognitionType} type The type of speech recognition used.
   * @private
   */
  onSpeechRecognitionStarted_(type) {
    if (chrome.runtime.lastError) {
      // chrome.runtime.lastError will be set if the call to
      // speechRecognitionPrivate.start() caused an error. When this happens,
      // the speech recognition private API will turn the associated recognizer
      // off. To align with this, we should call `stopDictation_`.
      this.stopDictation_(/*notify=*/ true);
      return;
    }

    if (!this.active_) {
      return;
    }

    this.clearInterimText_();

    // Record metrics.
    this.metricsUtils_ = new MetricsUtils(type, this.localePref_);
    this.metricsUtils_.recordSpeechRecognitionStarted();

    this.uiController_.setState(
        UIState.STANDBY, {context: HintContext.STANDBY});
  }

  /**
   * Called when speech recognition stops or when speech recognition encounters
   * an error.
   * @param {StopEvent} event
   * @private
   */
  onSpeechRecognitionStopped_(event) {
    if (this.metricsUtils_ !== null) {
      this.metricsUtils_.recordSpeechRecognitionStopped();
    }
    this.metricsUtils_ = null;

    // Stop dictation if it wasn't already stopped.
    this.stopDictation_(/*notify=*/ true);
  }

  /**
   * @param {ErrorEvent} event
   * @private
   */
  onSpeechRecognitionError_(event) {
    // TODO: Dictation does not surface speech recognition errors to the user.
    // Informing the user of errors, for example lack of network connection or a
    // missing microphone, would be a useful feature.
    this.stopDictation_(/*notify=*/ true);
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
            this.speechRecognitionOptions_.locale =
                /** @type {string} */ (pref.value);
            this.localePref_ = this.speechRecognitionOptions_.locale;
            this.speechParser_.initialize(this.localePref_);
          }
          break;
        case Dictation.SPOKEN_FEEDBACK_PREF:
          if (pref.value) {
            this.chromeVoxEnabled_ = true;
          } else {
            this.chromeVoxEnabled_ = false;
          }
          break;
        default:
          return;
      }
    });
  }

  /**
   * Shows the interim result in the UI.
   * @param {string} text
   * @private
   */
  showInterimText_(text) {
    // TODO(crbug.com/1252037): Need to find a way to show interim text that is
    // only whitespace. Google Cloud Speech can return a newline character
    // although SODA does not seem to do that. The newline character looks wrong
    // here.
    this.interimText_ = text;
    this.uiController_.setState(UIState.RECOGNIZING_TEXT, {text});
  }

  /**
   * Clears the interim result in the UI.
   * @private
   */
  clearInterimText_() {
    this.interimText_ = '';
    this.uiController_.setState(UIState.STANDBY);
  }

  /**
   * Shows that a macro was executed in the UI by putting a checkmark next to
   * the transcript.
   * @param {!Macro} macro
   * @param {string} transcript
   * @private
   */
  showMacroExecuted_(macro, transcript) {
    MetricsUtils.recordMacroSucceeded(macro);

    if (macro.getMacroName() === MacroName.INPUT_TEXT_VIEW ||
        macro.getMacroName() === MacroName.NEW_LINE) {
      this.clearInterimText_();
      this.uiController_.setState(
          UIState.STANDBY, {context: HintContext.TEXT_COMMITTED});
      return;
    }
    this.interimText_ = '';
    const context = macro.getMacroName() === MacroName.SELECT_ALL_TEXT ?
        HintContext.TEXT_SELECTED :
        HintContext.MACRO_SUCCESS;
    this.uiController_.setState(
        UIState.MACRO_SUCCESS, {text: transcript, context});
  }

  /**
   * Shows a message in the UI that a command failed to execute.
   * TODO(crbug.com/1252037): Optionally use the MacroError to provide
   * additional context.
   * @param {!Macro} macro
   * @param {string} transcript The user's spoken transcript, shown so they
   *     understand the final speech recognized which might be helpful in
   *     understanding why the command failed.
   * @private
   */
  showMacroExecutionFailed_(macro, transcript) {
    MetricsUtils.recordMacroFailed(macro);

    this.interimText_ = '';
    // TODO(crbug.com/1288964): Finalize string and internationalization.
    this.uiController_.setState(UIState.MACRO_FAIL, {
      text: `Failed to run command: ${transcript}`,
      context: HintContext.STANDBY
    });
  }

  /**
   * Hides the commands UI bubble.
   * @private
   */
  hideCommandsUI_() {
    this.interimText_ = '';
    this.uiController_.setState(UIState.HIDDEN);
  }

  /** @private */
  clearTimeoutIds_() {
    if (this.stopTimeoutId_ !== null) {
      clearTimeout(this.stopTimeoutId_);
      this.stopTimeoutId_ = null;
    }
  }

  /**
   * Removes AccessibilityCommon as an input method so it doesn't show up in
   * the shelf input method picker UI.
   */
  static removeAsInputMethod() {
    chrome.languageSettingsPrivate.removeInputMethod(
        InputController.IME_ENGINE_ID);
  }
}

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
Dictation.Timeouts = {
  NO_SPEECH_MS: 10 * 1000,
  NO_NEW_SPEECH_MS: 5 * 1000,
  NO_FOCUSED_IME_MS: 500,
};

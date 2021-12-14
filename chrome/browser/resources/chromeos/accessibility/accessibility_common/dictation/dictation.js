// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InputController} from './input_controller.js';
import {Macro} from './macros/macro.js';
import {MacroName} from './macros/macro_names.js';
import {MetricsUtils} from './metrics_utils.js';
import {SpeechParser} from './parse/speech_parser.js';

const ErrorEvent = chrome.speechRecognitionPrivate.SpeechRecognitionErrorEvent;
const ResultEvent =
    chrome.speechRecognitionPrivate.SpeechRecognitionResultEvent;
const StartOptions = chrome.speechRecognitionPrivate.StartOptions;
const StopEvent = chrome.speechRecognitionPrivate.SpeechRecognitionStopEvent;
const SpeechRecognitionType =
    chrome.speechRecognitionPrivate.SpeechRecognitionType;

/**
 * Main class for the Chrome OS dictation feature.
 * Please note: this is being developed behind the flag
 * --enable-experimental-accessibility-dictation-extension
 */
export class Dictation {
  constructor() {
    /** @private {InputController} */
    this.inputController_ = null;

    /** @private {SpeechParser} */
    this.speechParser_ = null;

    /** @private {boolean} */
    this.commandsFeatureEnabled_ = false;

    /** @private {string} */
    this.localePref_ = '';

    /**
     * The state of Dictation.
     * @private {!Dictation.DictationState}
     */
    this.state_ = Dictation.DictationState.OFF;

    /** @private {Audio} */
    this.cancelTone_ = new Audio('dictation/earcons/null_selection.wav');

    /** @private {Audio} */
    this.startTone_ = new Audio('dictation/earcons/audio_initiate.wav');

    /** @private {Audio} */
    this.endTone_ = new Audio('dictation/earcons/audio_end.wav');

    /** @private {?number} */
    this.timeoutId_ = null;

    /** @private {?number} */
    this.clearUITextTimeoutId_ = null;

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
    this.inputController_ = new InputController(() => this.stopDictation_());
    this.speechParser_ = new SpeechParser(this.inputController_);

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

    chrome.accessibilityPrivate.isFeatureEnabled(
        chrome.accessibilityPrivate.AccessibilityFeature.DICTATION_COMMANDS,
        (result) => {
          this.commandsFeatureEnabled_ = result;
          if (this.commandsFeatureEnabled_ && this.localePref_) {
            this.speechParser_.setCommandsEnabled(this.localePref_);
          }
        });
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
      this.setStopTimeout_(Dictation.Timeouts.NO_FOCUSED_IME_MS);
      this.inputController_.connect(() => this.maybeStartSpeechRecognition_());
    } else {
      this.onDictationStopped_();
    }
  }

  /**
   * Sets the timeout to stop Dictation.
   * @param {number} durationMs
   * @private
   */
  setStopTimeout_(durationMs) {
    if (this.timeoutId_ !== null) {
      clearTimeout(this.timeoutId_);
    }
    this.timeoutId_ = setTimeout(() => this.stopDictation_(), durationMs);
  }

  /**
   * Called when Dictation has set itself as the IME during start-up. Because
   * this is async, checks that startup state is still correct before starting
   * speech recognition.
   * @private
   */
  maybeStartSpeechRecognition_() {
    if (this.state_ === Dictation.DictationState.STARTING) {
      chrome.speechRecognitionPrivate.start(
          /** @type {!StartOptions} */ (this.speechRecognitionOptions_),
          (type) => this.onSpeechRecognitionStarted_(type));
      this.setStopTimeout_(Dictation.Timeouts.NO_SPEECH_MS);
    } else {
      // We are no longer starting up - perhaps a stop came
      // through during the async callbacks. Ensure cleanup
      // by calling stopDictation_().
      this.stopDictation_();
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
    if (this.state_ === Dictation.DictationState.OFF ||
        this.state_ === Dictation.DictationState.STOPPING) {
      return;
    }

    chrome.accessibilityPrivate.toggleDictation();
    this.state_ = Dictation.DictationState.STOPPING;
  }

  /**
   * Called when Dictation has been toggled off. Cleans up IME, local state,
   * and speech recognition.
   * @private
   */
  onDictationStopped_() {
    if (this.state_ === Dictation.DictationState.OFF) {
      return;
    }

    this.state_ = Dictation.DictationState.OFF;
    chrome.speechRecognitionPrivate.stop({}, () => {});
    if (this.inputController_.hasCompositionText() || this.interimText_) {
      this.endTone_.play();
    } else {
      this.cancelTone_.play();
    }

    // Clear any timeouts.
    if (this.timeoutId_ !== null) {
      clearTimeout(this.timeoutId_);
      this.timeoutId_ = null;
    }

    if (this.commandsFeatureEnabled_) {
      this.inputController_.commitText(this.interimText_);
      this.hideCommandsUI_();
    }
    this.inputController_.disconnect();
    Dictation.removeAsInputMethod();
  }

  /**
   * Called when the Speech Recognition engine receives a recognition event.
   * @param {ResultEvent} event
   * @private
   */
  async onSpeechRecognitionResult_(event) {
    if (this.state_ !== Dictation.DictationState.LISTENING) {
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
    // TODO(crbug.com/1216111): Make dictation.js store the current composition
    // (we already have a member called interimText_) and remove the
    // currentComposition_ member from input_controller.js. This aligns more
    // closely with the model-view-controller design pattern.
    this.inputController_.setCurrentComposition(transcript);

    if (!isFinal) {
      if (this.commandsFeatureEnabled_) {
        this.setInterimText_(transcript);
      } else if (!this.chromeVoxEnabled_) {
        // When ChromeVox is enabled, we shouldn't display interim
        // composition results because it will increase the verbosity too much.
        this.inputController_.displayCurrentComposition();
      }
      return;
    }

    const macro = await this.speechParser_.parse(transcript);
    // Check if the macro can execute.
    // TODO(crbug.com/1264544): Deal with ambiguous results here.
    const checkContextResult = macro.checkContext();
    if (!checkContextResult.canTryAction) {
      this.showMacroExecutionFailed_(transcript);
      return;
    }

    // Try to run the macro.
    const runMacroResult = macro.runMacro();
    if (!runMacroResult.isSuccess) {
      this.showMacroExecutionFailed_(transcript);
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
      this.stopDictation_();
      return;
    }

    if (this.state_ !== Dictation.DictationState.STARTING) {
      // We tried to stop during speech shutdown.
      return;
    }

    this.state_ = Dictation.DictationState.LISTENING;
    this.clearInterimText_();

    // Record metrics.
    this.metricsUtils_ = new MetricsUtils(type, this.localePref_);
    this.metricsUtils_.recordSpeechRecognitionStarted();
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
    this.stopDictation_();
  }

  /**
   * @param {ErrorEvent} event
   * @private
   */
  onSpeechRecognitionError_(event) {
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
            this.speechRecognitionOptions_.locale =
                /** @type {string} */ (pref.value);
            this.localePref_ = this.speechRecognitionOptions_.locale;
            if (this.commandsFeatureEnabled_) {
              this.speechParser_.setCommandsEnabled(this.localePref_);
            }
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
  setInterimText_(text) {
    if (this.chromeVoxEnabled_ || !this.commandsFeatureEnabled_) {
      // Using chrome.input.ime for UI causes too much verbosity with ChromeVox.
      return;
    }

    // TODO(crbug.com/1252037): Need to find a way to show interim text that is
    // only whitespace. Google Cloud Speech can return a newline character
    // although SODA does not seem to do that. The newline character looks wrong
    // here.
    this.interimText_ = text;
    this.inputController_.showBubble(this.interimText_);
    if (this.clearUITextTimeoutId_) {
      clearTimeout(this.clearUITextTimeoutId_);
      this.clearUITextTimeoutId_ = null;
    }
  }

  /**
   * Clears the interim result in the UI.
   * @private
   */
  clearInterimText_() {
    if (this.chromeVoxEnabled_ || !this.commandsFeatureEnabled_) {
      // Using chrome.input.ime for UI causes too much verbosity with ChromeVox.
      return;
    }

    this.interimText_ = '';
    this.inputController_.showBubble('');
    if (this.clearUITextTimeoutId_) {
      clearTimeout(this.clearUITextTimeoutId_);
      this.clearUITextTimeoutId_ = null;
    }
  }

  /**
   * Shows that a macro was executed in the UI by putting a checkmark next to
   * the transcript.
   * @param {Macro} macro
   * @param {string} transcript
   * @private
   */
  showMacroExecuted_(macro, transcript) {
    if (this.chromeVoxEnabled_ || !this.commandsFeatureEnabled_) {
      // Using chrome.input.ime for UI causes too much verbosity with ChromeVox.
      return;
    }

    if (macro.getMacroName() === MacroName.INPUT_TEXT_VIEW ||
        macro.getMacroName() === MacroName.NEW_LINE) {
      this.clearInterimText_();
      return;
    }
    this.interimText_ = '';
    this.inputController_.showBubble('☑' + transcript);
    this.clearUITextTimeoutId_ = setTimeout(
        () => this.clearInterimText_(),
        Dictation.Timeouts.SHOW_COMMAND_MESSAGE_MS);
  }

  /**
   * Shows a message in the UI that a command failed to execute.
   * TODO(crbug.com/1252037): Optionally use the MacroError to provide
   * additional context.
   * @param {string} transcript The user's spoken transcript, shown so they
   *     understand the final speech recognized which might be helpful in
   *     understanding why the command failed.
   * @private
   */
  showMacroExecutionFailed_(transcript) {
    if (this.chromeVoxEnabled_ || !this.commandsFeatureEnabled_) {
      // Using chrome.input.ime for UI causes too much verbosity with ChromeVox.
      return;
    }

    this.interimText_ = '';
    // TODO(crbug.com/1252037): Finalize string and internationalization.
    this.inputController_.showBubble(`ⓘ Failed to execute: ` + transcript);
    this.clearUITextTimeoutId_ = setTimeout(
        () => this.clearInterimText_(),
        Dictation.Timeouts.SHOW_COMMAND_MESSAGE_MS);
  }

  /**
   * Hides the commands UI bubble.
   * @private
   */
  hideCommandsUI_() {
    if (this.chromeVoxEnabled_ || !this.commandsFeatureEnabled_) {
      return;
    }

    this.interimText_ = '';
    this.inputController_.hideBubble();
    if (this.clearUITextTimeoutId_) {
      clearTimeout(this.clearUITextTimeoutId_);
      this.clearUITextTimeoutId_ = null;
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
 * Dictation states.
 * @enum {!string}
 */
Dictation.DictationState = {
  OFF: 'OFF',
  STARTING: 'STARTING',
  LISTENING: 'LISTENING',
  STOPPING: 'STOPPING',
};

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
  SHOW_COMMAND_MESSAGE_MS: 2000,
};

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Context} from './context_checker.js';
import {FocusHandler} from './focus_handler.js';
import {InputController} from './input_controller.js';
import {LocaleInfo} from './locale_info.js';
import {Macro} from './macros/macro.js';
import {MacroName} from './macros/macro_names.js';
import {MetricsUtils} from './metrics_utils.js';
import {SpeechParser} from './parse/speech_parser.js';
import {HintContext, UIController, UIState} from './ui_controller.js';

const ErrorEvent = chrome.speechRecognitionPrivate.SpeechRecognitionErrorEvent;
const ResultEvent =
    chrome.speechRecognitionPrivate.SpeechRecognitionResultEvent;
const StartOptions = chrome.speechRecognitionPrivate.StartOptions;
const StopEvent = chrome.speechRecognitionPrivate.SpeechRecognitionStopEvent;
const StreamType = chrome.audio.StreamType;
const SpeechRecognitionType =
    chrome.speechRecognitionPrivate.SpeechRecognitionType;
const PrefObject = chrome.settingsPrivate.PrefObject;
const ToastType = chrome.accessibilityPrivate.ToastType;

/** Main class for the Chrome OS dictation feature. */
export class Dictation {
  constructor() {
    /** @private {InputController} */
    this.inputController_ = null;

    /** @private {UIController} */
    this.uiController_ = null;

    /** @private {SpeechParser} */
    this.speechParser_ = null;

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

    /** @private {number} */
    this.noSpeechTimeoutMs_ = Dictation.Timeouts.NO_SPEECH_NETWORK_MS;

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

    /** @private {?FocusHandler} */
    this.focusHandler_ = null;

    // API Listeners //

    /** @private {?function(StopEvent):void} */
    this.speechRecognitionStopListener_ = null;

    /** @private {?function(ResultEvent):Promise} */
    this.speechRecognitionResultListener_ = null;

    /** @private {?function(ErrorEvent):void} */
    this.speechRecognitionErrorListener_ = null;

    /** @private {?function(!Array<!PrefObject>):void} */
    this.prefsListener_ = null;

    /** @private {?function(boolean):void} */
    this.onToggleDictationListener_ = null;

    /** @private {boolean} */
    this.isContextCheckingFeatureEnabled_ = false;

    /** @private {Macro} */
    this.prevMacro_ = null;

    this.initialize_();
  }

  /**
   * Sets up Dictation's speech recognizer and various listeners.
   * @private
   */
  initialize_() {
    this.focusHandler_ = new FocusHandler();
    this.inputController_ = new InputController(
        () => this.stopDictation_(/*notify=*/ true), this.focusHandler_);
    this.uiController_ = new UIController();
    this.speechParser_ = new SpeechParser(this.inputController_);
    this.speechParser_.refresh();

    // Set default speech recognition properties. Locale will be updated when
    // `updateFromPrefs_` is called.
    this.speechRecognitionOptions_ = {
      locale: 'en-US',
      interimResults: true,
    };

    this.speechRecognitionStopListener_ = event =>
        this.onSpeechRecognitionStopped_(event);
    this.speechRecognitionResultListener_ = event =>
        this.onSpeechRecognitionResult_(event);
    this.speechRecognitionErrorListener_ = event =>
        this.onSpeechRecognitionError_(event);
    this.prefsListener_ = prefs => this.updateFromPrefs_(prefs);
    this.onToggleDictationListener_ = activated =>
        this.onToggleDictation_(activated);

    // Setup speechRecognitionPrivate API listeners.
    chrome.speechRecognitionPrivate.onStop.addListener(
        this.speechRecognitionStopListener_);
    chrome.speechRecognitionPrivate.onResult.addListener(
        this.speechRecognitionResultListener_);
    chrome.speechRecognitionPrivate.onError.addListener(
        this.speechRecognitionErrorListener_);

    chrome.settingsPrivate.getAllPrefs(prefs => this.updateFromPrefs_(prefs));
    chrome.settingsPrivate.onPrefsChanged.addListener(this.prefsListener_);

    // Listen for Dictation toggles (activated / deactivated) from the Ash
    // Browser process.
    chrome.accessibilityPrivate.onToggleDictation.addListener(
        this.onToggleDictationListener_);

    const contextCheckingFeature =
        chrome.accessibilityPrivate.AccessibilityFeature
            .DICTATION_CONTEXT_CHECKING;
    chrome.accessibilityPrivate.isFeatureEnabled(
        contextCheckingFeature, enabled => {
          this.isContextCheckingFeatureEnabled_ = enabled;
        });
  }

  /**
   * Performs any destruction before dictation object is destroyed.
   */
  onDictationDisabled() {
    if (this.speechRecognitionStopListener_) {
      chrome.speechRecognitionPrivate.onStop.removeListener(
          this.speechRecognitionStopListener_);
    }
    if (this.speechRecognitionResultListener_) {
      chrome.speechRecognitionPrivate.onResult.removeListener(
          this.speechRecognitionResultListener_);
    }
    if (this.speechRecognitionErrorListener_) {
      chrome.speechRecognitionPrivate.onError.removeListener(
          this.speechRecognitionErrorListener_);
    }
    if (this.prefsListener_) {
      chrome.settingsPrivate.onPrefsChanged.removeListener(this.prefsListener_);
    }
    if (this.onToggleDictationListener_) {
      chrome.accessibilityPrivate.onToggleDictation.removeListener(
          this.onToggleDictationListener_);
    }
    if (this.inputController_) {
      this.inputController_.removeListeners();
    }
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
    if (this.chromeVoxEnabled_) {
      // Silence ChromeVox in case it was speaking. It can speak over the start
      // tone and also cause a feedback loop if the user is not using
      // headphones. This does not stop ChromeVox from speaking additional
      // utterances added to the queue later.
      chrome.accessibilityPrivate.silenceSpokenFeedback();
    }
    this.setStopTimeout_(
        Dictation.Timeouts.NO_FOCUSED_IME_MS,
        Dictation.StopReason.NO_FOCUSED_IME);
    this.inputController_.connect(() => this.verifyMicrophoneNotMuted_());
  }

  /**
   * Checks if the microphone is muted. If it is, then we stop Dictation and
   * show a notification to the user. If the microphone isn't muted, then we
   * proceed to start speech recognition. Because this is async, this method
   * checks that startup state is still correct before proceeding.
   * @private
   */
  verifyMicrophoneNotMuted_() {
    if (!this.active_) {
      this.stopDictation_(/*notify=*/ true);
      return;
    }

    // TODO(b:299677121): Determine if it's possible for no mics to be
    // available. If that scenario is possible, we may have to use
    // `chrome.audio.getDevices` and verify that there's at least one input
    // device.
    chrome.audio.getMute(StreamType.INPUT, muted => {
      if (muted) {
        this.stopDictation_(/*notify=*/ true);
        chrome.accessibilityPrivate.showToast(ToastType.DICTATION_MIC_MUTED);
        return;
      }

      this.maybeStartSpeechRecognition_();
    });
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
          type => this.onSpeechRecognitionStarted_(type));
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
    this.clearStopTimeout_();

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
   * @param {Dictation.StopReason=} reason Optional reason for why Dictation
   *     stopped automatically.
   * @private
   */
  setStopTimeout_(durationMs, reason) {
    if (this.stopTimeoutId_ !== null) {
      clearTimeout(this.stopTimeoutId_);
    }
    this.stopTimeoutId_ = setTimeout(() => {
      this.stopDictation_(/*notify=*/ true);

      if (reason === Dictation.StopReason.NO_FOCUSED_IME) {
        chrome.accessibilityPrivate.showToast(
            ToastType.DICTATION_NO_FOCUSED_TEXT_FIELD);
      }
    }, durationMs);
  }

  /**
   * Called when the Speech Recognition engine receives a recognition event.
   * @param {ResultEvent} event
   * @return {!Promise}
   * @private
   */
  async onSpeechRecognitionResult_(event) {
    if (!this.active_) {
      return;
    }

    const transcript = event.transcript;
    const isFinal = event.isFinal;
    this.setStopTimeout_(
        isFinal ? this.noSpeechTimeoutMs_ :
                  Dictation.Timeouts.NO_NEW_SPEECH_MS);
    await this.processSpeechRecognitionResult_(transcript, isFinal);
  }

  /**
   * Processes a speech recognition result.
   * @param {string} transcript
   * @param {boolean} isFinal Whether this is a finalized transcript or an
   *     interim result.
   * @return {!Promise}
   * @private
   */
  async processSpeechRecognitionResult_(transcript, isFinal) {
    if (!isFinal) {
      this.showInterimText_(transcript);
      return;
    }

    let macro = await this.speechParser_.parse(transcript);
    MetricsUtils.recordMacroRecognized(macro);
    macro = this.handleRepeat_(macro);

    // Check if the macro can execute.
    // TODO(crbug.com/1264544): Deal with ambiguous results here.
    const checkContextResult = macro.checkContext();
    if (!checkContextResult.canTryAction &&
        this.isContextCheckingFeatureEnabled_) {
      this.showMacroExecutionFailed_(
          macro, transcript, checkContextResult.failedContext);
      return;
    }

    // Try to run the macro.
    const runMacroResult = macro.run();
    if (!runMacroResult.isSuccess) {
      this.showMacroExecutionFailed_(macro, transcript);
      return;
    }
    if (macro.getName() === MacroName.LIST_COMMANDS) {
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

    this.noSpeechTimeoutMs_ = type === SpeechRecognitionType.NETWORK ?
        Dictation.Timeouts.NO_SPEECH_NETWORK_MS :
        Dictation.Timeouts.NO_SPEECH_ONDEVICE_MS;
    this.setStopTimeout_(this.noSpeechTimeoutMs_);

    this.startTone_.play();
    this.clearInterimText_();

    // Record metrics.
    this.metricsUtils_ = new MetricsUtils(type, LocaleInfo.locale);
    this.metricsUtils_.recordSpeechRecognitionStarted();

    this.uiController_.setState(
        UIState.STANDBY, {context: HintContext.STANDBY});
    this.focusHandler_.refresh();
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
   * @param {!Array<!PrefObject>} prefs
   * @private
   */
  updateFromPrefs_(prefs) {
    prefs.forEach(pref => {
      switch (pref.key) {
        case Dictation.DICTATION_LOCALE_PREF:
          if (pref.value) {
            const locale = /** @type {string} */ (pref.value);
            this.speechRecognitionOptions_.locale = locale;
            LocaleInfo.locale = locale;
            this.speechParser_.refresh();
          }
          break;
        case Dictation.SPOKEN_FEEDBACK_PREF:
          if (pref.value) {
            this.chromeVoxEnabled_ = true;
          } else {
            this.chromeVoxEnabled_ = false;
          }
          // Use a longer hints timeout when ChromeVox is enabled.
          this.uiController_.setHintsTimeoutDuration(this.chromeVoxEnabled_);
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

    if (macro.getName() === MacroName.INPUT_TEXT_VIEW ||
        macro.getName() === MacroName.NEW_LINE) {
      this.clearInterimText_();
      this.uiController_.setState(
          UIState.STANDBY, {context: HintContext.TEXT_COMMITTED});
      return;
    }
    this.interimText_ = '';
    const context = macro.getName() === MacroName.SELECT_ALL_TEXT ?
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
   * @param {!Context=} failedContext
   * @private
   */
  showMacroExecutionFailed_(macro, transcript, failedContext) {
    MetricsUtils.recordMacroFailed(macro);

    this.interimText_ = '';
    let text = '';
    if (!failedContext) {
      text = chrome.i18n.getMessage(
          'dictation_command_failed_generic', [transcript]);
    } else {
      const reason = Dictation.getFailedContextReason(failedContext);
      text = chrome.i18n.getMessage(
          'dictation_command_failed_with_reason', [transcript, reason]);
    }

    this.uiController_.setState(UIState.MACRO_FAIL, {
      text,
      context: HintContext.STANDBY,
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
  clearStopTimeout_() {
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

  /** Used to set the NO_FOCUSED_IME_MS timeout for testing purposes only. */
  setNoFocusedImeTimeoutForTesting(duration) {
    Dictation.Timeouts.NO_FOCUSED_IME_MS = duration;
  }

  /**
   * @param {!Macro} macro
   * @return {!Macro}
   * @private
   */
  handleRepeat_(macro) {
    if (macro.getName() !== MacroName.REPEAT) {
      // If this macro is not the RepeatMacro, save it and return the existing
      // macro.
      this.prevMacro_ = macro;
      return macro;
    }

    // Handle cases where `macro` is the RepeatMacro.
    if (!this.prevMacro_) {
      // If there is no previous macro, return the RepeatMacro.
      return macro;
    }

    // Otherwise, return the previous macro.
    return this.prevMacro_;
  }

  /** Disables Pumpkin for tests that use regex-based command parsing. */
  disablePumpkinForTesting() {
    this.speechParser_.disablePumpkinForTesting();
  }

  /**
   * @param {!Context} context
   * @return {string}
   */
  static getFailedContextReason(context) {
    switch (context) {
      case Context.INACTIVE_INPUT_CONTROLLER:
        return chrome.i18n.getMessage(
            'dictation_context_error_reason_inactive_input_controller');
      case Context.EMPTY_EDITABLE:
        return chrome.i18n.getMessage(
            'dictation_context_error_reason_empty_editable');
      case Context.NO_SELECTION:
        return chrome.i18n.getMessage(
            'dictation_context_error_reason_no_selection');
      case Context.INVALID_INPUT:
        return chrome.i18n.getMessage(
            'dictation_context_error_reason_invalid_input');
      case Context.NO_PREVIOUS_MACRO:
        return chrome.i18n.getMessage(
            'dictation_context_error_reason_no_previous_macro');
    }

    throw new Error(
        'Cannot get error message for unsupported context: ' + context);
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
  NO_SPEECH_NETWORK_MS: 10 * 1000,
  NO_SPEECH_ONDEVICE_MS: 20 * 1000,
  NO_NEW_SPEECH_MS: 5 * 1000,
  NO_FOCUSED_IME_MS: 1000,
};

/** @enum {string} */
Dictation.StopReason = {
  NO_FOCUSED_IME: 'Dictation stopped automatically: No focused IME',
};

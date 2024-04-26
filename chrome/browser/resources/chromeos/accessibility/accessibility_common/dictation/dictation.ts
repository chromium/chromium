// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Context} from '/common/action_fulfillment/context_checker.js';
import {Macro} from '/common/action_fulfillment/macros/macro.js';
import {MacroName} from '/common/action_fulfillment/macros/macro_names.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {FocusHandler} from './focus_handler.js';
import {InputControllerImpl} from './input_controller_impl.js';
import {LocaleInfo} from './locale_info.js';
import {MetricsUtils} from './metrics_utils.js';
import {SpeechParser} from './parse/speech_parser.js';
import {HintContext, UIController, UIState} from './ui_controller.js';

type ErrorEvent = chrome.speechRecognitionPrivate.SpeechRecognitionErrorEvent;
type ResultEvent = chrome.speechRecognitionPrivate.SpeechRecognitionResultEvent;
type StartOptions = chrome.speechRecognitionPrivate.StartOptions;
type StopEvent = chrome.speechRecognitionPrivate.SpeechRecognitionStopEvent;
import StreamType = chrome.audio.StreamType;
import SpeechRecognitionType = chrome.speechRecognitionPrivate.SpeechRecognitionType;
type PrefObject = chrome.settingsPrivate.PrefObject;
import ToastType = chrome.accessibilityPrivate.ToastType;

/**
 * Main class for the Chrome OS dictation feature.
 * TODO(b/314204374): Eliminate instances of null.
 */
export class Dictation {
  private inputController_: InputControllerImpl|null = null;
  private uiController_: UIController|null = null;
  private speechParser_: SpeechParser|null = null;
  /** Whether or not Dictation is active. */
  private active_ = false;
  private cancelTone_: HTMLAudioElement|null =
      new Audio('dictation/earcons/null_selection.wav');
  private startTone_: HTMLAudioElement|null =
      new Audio('dictation/earcons/audio_initiate.wav');
  private endTone_: HTMLAudioElement|null =
      new Audio('dictation/earcons/audio_end.wav');
  private noSpeechTimeoutMs_: number = Dictation.Timeouts.NO_SPEECH_NETWORK_MS;
  private stopTimeoutId_: number|null = null;
  private interimText_ = '';
  private chromeVoxEnabled_ = false;
  private speechRecognitionOptions_: StartOptions|null = null;
  private metricsUtils_: MetricsUtils|null = null;
  private focusHandler_: FocusHandler|null = null;
  // API Listeners //
  private speechRecognitionStopListener_:
      ((event: StopEvent) => void)|null = null;
  private speechRecognitionResultListener_:
      ((event: ResultEvent) => Promise<void>)|null = null;
  private speechRecognitionErrorListener_:
      ((event: ErrorEvent) => void)|null = null;
  private prefsListener_: ((prefs: PrefObject[]) => void)|null = null;
  private onToggleDictationListener_: ((active: boolean) => void)|null = null;
  private isContextCheckingFeatureEnabled_ = false;
  private prevMacro_: Macro|null = null;

  constructor() {
    this.initialize_();
  }

  /** Sets up Dictation's speech recognizer and various listeners. */
  private initialize_(): void {
    this.focusHandler_ = new FocusHandler();
    this.inputController_ = new InputControllerImpl(
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

    this.speechRecognitionStopListener_ = () =>
        this.onSpeechRecognitionStopped_();
    this.speechRecognitionResultListener_ = event =>
        this.onSpeechRecognitionResult_(event);
    this.speechRecognitionErrorListener_ = () =>
        this.onSpeechRecognitionError_();
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

  /** Performs any destruction before dictation object is destroyed. */
  onDictationDisabled(): void {
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
   * @param activated Whether Dictation was just activated.
   */
  private onToggleDictation_(activated: boolean): void {
    if (activated && !this.active_) {
      this.startDictation_();
    } else {
      this.stopDictation_(/*notify=*/ false);
    }
  }

  private startDictation_(): void {
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
    // TODO(b/314203187): Determine if not null assertion is acceptable.
    this.inputController_!.connect(() => this.verifyMicrophoneNotMuted_());
  }

  /**
   * Checks if the microphone is muted. If it is, then we stop Dictation and
   * show a notification to the user. If the microphone isn't muted, then we
   * proceed to start speech recognition. Because this is async, this method
   * checks that startup state is still correct before proceeding.
   */
  private verifyMicrophoneNotMuted_(): void {
    if (!this.active_) {
      this.stopDictation_(/*notify=*/ true);
      return;
    }

    // TODO(b:299677121): Determine if it's possible for no mics to be
    // available. If that scenario is possible, we may have to use
    // `chrome.audio.getDevices` and verify that there's at least one input
    // device.
    chrome.audio.getMute(StreamType.INPUT, (muted: boolean) => {
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
   */
  private maybeStartSpeechRecognition_(): void {
    if (this.active_) {
      // TODO(b/314203187): Determine if not null assertion is acceptable.
      chrome.speechRecognitionPrivate.start(
          this.speechRecognitionOptions_!,
          (type: SpeechRecognitionType) =>
              this.onSpeechRecognitionStarted_(type));
    } else {
      // We are no longer starting up - perhaps a stop came
      // through during the async callbacks. Ensure cleanup
      // by calling stopDictation_().
      this.stopDictation_(/*notify=*/ true);
    }
  }

  /**
   * Stops Dictation and notifies the browser.
   * @param notify True if we should notify the browser that Dictation
   * stopped.
   */
  private stopDictation_(notify: boolean): void {
    if (!this.active_) {
      return;
    }

    this.active_ = false;
    // Stop speech recognition.
    chrome.speechRecognitionPrivate.stop({}, () => {});
    if (this.interimText_) {
      // TODO(b/314203187): Determine if not null assertion is acceptable.
      this.endTone_!.play();
    } else {
      // TODO(b/314203187): Determine if not null assertion is acceptable.
      this.cancelTone_!.play();
    }

    // Clear any timeouts.
    this.clearStopTimeout_();

    // TODO(b/314203187): Determine if not null assertion is acceptable.
    this.inputController_!.commitText(this.interimText_);
    this.hideCommandsUI_();
    this.inputController_!.disconnect();
    Dictation.removeAsInputMethod();

    // Notify the browser that Dictation turned off.
    if (notify) {
      chrome.accessibilityPrivate.toggleDictation();
    }
  }

  /**
   * Sets the timeout to stop Dictation.
   * @param reason Optional reason for why Dictation
   *     stopped automatically.
   */
  private setStopTimeout_(durationMs: number, reason?: Dictation.StopReason):
      void {
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

  /** Called when the Speech Recognition engine receives a recognition event. */
  private async onSpeechRecognitionResult_(event: ResultEvent): Promise<void> {
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
   * @param isFinal Whether this is a finalized transcript or an
   *     interim result.
   */
  private async processSpeechRecognitionResult_(
      transcript: string, isFinal: boolean): Promise<void> {
    if (!isFinal) {
      this.showInterimText_(transcript);
      return;
    }

    // TODO(b/314203187): Determine if not null assertion is acceptable.
    let macro = await this.speechParser_!.parse(transcript);
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
   * @param type The type of speech recognition used.
   */
  private onSpeechRecognitionStarted_(type: SpeechRecognitionType): void {
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

    // TODO(b/314203187): Determine if not null assertion is acceptable.
    this.startTone_!.play();
    this.clearInterimText_();

    // Record metrics.
    this.metricsUtils_ = new MetricsUtils(type, LocaleInfo.locale);
    this.metricsUtils_.recordSpeechRecognitionStarted();

    // TODO(b/314203187): Determine if not null assertion is acceptable.
    this.uiController_!.setState(
        UIState.STANDBY, {context: HintContext.STANDBY});
    this.focusHandler_!.refresh();
  }

  /**
   * Called when speech recognition stops or when speech recognition encounters
   * an error.
   */
  private onSpeechRecognitionStopped_(): void {
    if (this.metricsUtils_ !== null) {
      this.metricsUtils_.recordSpeechRecognitionStopped();
    }
    this.metricsUtils_ = null;

    // Stop dictation if it wasn't already stopped.
    this.stopDictation_(/*notify=*/ true);
  }

  private onSpeechRecognitionError_(): void {
    // TODO: Dictation does not surface speech recognition errors to the user.
    // Informing the user of errors, for example lack of network connection or a
    // missing microphone, would be a useful feature.
    this.stopDictation_(/*notify=*/ true);
  }

  private updateFromPrefs_(prefs: PrefObject[]): void {
    prefs.forEach(pref => {
      switch (pref.key) {
        case Dictation.DICTATION_LOCALE_PREF:
          if (pref.value) {
            const locale = pref.value;
            // TODO(b/314203187): Determine if not null assertion is acceptable.
            this.speechRecognitionOptions_!.locale = locale;
            LocaleInfo.locale = locale;
            this.speechParser_!.refresh();
          }
          break;
        case Dictation.SPOKEN_FEEDBACK_PREF:
          if (pref.value) {
            this.chromeVoxEnabled_ = true;
          } else {
            this.chromeVoxEnabled_ = false;
          }
          // Use a longer hints timeout when ChromeVox is enabled.
          // TODO(b/314203187): Determine if not null assertion is acceptable.
          this.uiController_!.setHintsTimeoutDuration(this.chromeVoxEnabled_);
          break;
        default:
          return;
      }
    });
  }

  /** Shows the interim result in the UI. */
  private showInterimText_(text: string): void {
    // TODO(crbug.com/40792919): Need to find a way to show interim text that is
    // only whitespace. Google Cloud Speech can return a newline character
    // although SODA does not seem to do that. The newline character looks wrong
    // here.
    this.interimText_ = text;
    // TODO(b/314203187): Determine if not null assertion is acceptable.
    this.uiController_!.setState(UIState.RECOGNIZING_TEXT, {text});
  }

  /** Clears the interim result in the UI. */
  private clearInterimText_(): void {
    this.interimText_ = '';
    // TODO(b/314203187): Determine if not null assertion is acceptable.
    this.uiController_!.setState(UIState.STANDBY);
  }

  /**
   * Shows that a macro was executed in the UI by putting a checkmark next to
   * the transcript.
   */
  private showMacroExecuted_(macro: Macro, transcript: string): void {
    MetricsUtils.recordMacroSucceeded(macro);

    if (macro.getName() === MacroName.INPUT_TEXT_VIEW ||
        macro.getName() === MacroName.NEW_LINE) {
      this.clearInterimText_();
      // TODO(b/314203187): Determine if not null assertion is acceptable.
      this.uiController_!.setState(
          UIState.STANDBY, {context: HintContext.TEXT_COMMITTED});
      return;
    }
    this.interimText_ = '';
    const context = macro.getName() === MacroName.SELECT_ALL_TEXT ?
        HintContext.TEXT_SELECTED :
        HintContext.MACRO_SUCCESS;
    // TODO(b/314203187): Determine if not null assertion is acceptable.
    this.uiController_!.setState(
        UIState.MACRO_SUCCESS, {text: transcript, context});
  }

  /**
   * Shows a message in the UI that a command failed to execute.
   * TODO(crbug.com/40792919): Optionally use the MacroError to provide
   * additional context.
   * @param transcript The user's spoken transcript, shown so they
   *     understand the final speech recognized which might be helpful in
   *     understanding why the command failed.
   */
  private showMacroExecutionFailed_(
      macro: Macro, transcript: string, failedContext?: Context): void {
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

    // TODO(b/314203187): Determine if not null assertion is acceptable.
    this.uiController_!.setState(UIState.MACRO_FAIL, {
      text,
      context: HintContext.STANDBY,
    });
  }

  /**
   * Hides the commands UI bubble.
   */
  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  private hideCommandsUI_(): void {
    this.interimText_ = '';
    // TODO(b/314203187): Determine if not null assertion is acceptable.
    this.uiController_!.setState(UIState.HIDDEN);
  }

  private clearStopTimeout_(): void {
    if (this.stopTimeoutId_ !== null) {
      clearTimeout(this.stopTimeoutId_);
      this.stopTimeoutId_ = null;
    }
  }

  /**
   * Removes AccessibilityCommon as an input method so it doesn't show up in
   * the shelf input method picker UI.
   */
  static removeAsInputMethod(): void {
    chrome.languageSettingsPrivate.removeInputMethod(
        InputControllerImpl.IME_ENGINE_ID);
  }

  /** Used to set the NO_FOCUSED_IME_MS timeout for testing purposes only. */
  setNoFocusedImeTimeoutForTesting(duration: number): void {
    Dictation.Timeouts.NO_FOCUSED_IME_MS = duration;
  }

  private handleRepeat_(macro: Macro): Macro {
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
  disablePumpkinForTesting(): void {
    // TODO(b/314203187): Determine if not null assertion is acceptable.
    this.speechParser_!.disablePumpkinForTesting();
  }

  static getFailedContextReason(context: Context): string {
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
      default:
        break;
    }

    throw new Error(
        'Cannot get error message for unsupported context: ' + context);
  }
}

export namespace Dictation {
  /** Dictation locale pref. */
  export const DICTATION_LOCALE_PREF: string = 'settings.a11y.dictation_locale';

  /** ChromeVox enabled pref. */
  export const SPOKEN_FEEDBACK_PREF: string = 'settings.accessibility';

  /** Timeout durations. */
  export const Timeouts = {
    NO_SPEECH_NETWORK_MS: 10 * 1000,
    NO_SPEECH_ONDEVICE_MS: 20 * 1000,
    NO_NEW_SPEECH_MS: 5 * 1000,
    NO_FOCUSED_IME_MS: 1000,
  };

  export enum StopReason {
    NO_FOCUSED_IME = 'Dictation stopped automatically: No focused IME',
  }
}

TestImportManager.exportForTesting(Dictation);

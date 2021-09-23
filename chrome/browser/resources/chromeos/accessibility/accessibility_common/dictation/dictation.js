// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Command, CommandParser} from './commands.js';
import {InputController} from './input_controller.js';

/**
 * Main class for the Chrome OS dictation feature.
 * Please note: this is being developed behind the flag
 * --enable-experimental-accessibility-dictation-extension
 */
// TODO(crbug.com/1216111): Metrics and offline speech recognition
// functionality.
export class Dictation {
  constructor() {
    /** @private {InputController} */
    this.inputController_ = null;

    /** @private {CommandParser} */
    this.commandParser_ = null;

    /** @private {boolean} */
    this.commandsFeatureEnabled_ = false;

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

    /** @private {?number} */
    this.clearUITextTimeoutId_ = null;

    /** @private {string} */
    this.interimText_ = '';

    /** @private {boolean} */
    this.chromeVoxEnabled_ = false;

    this.initialize_();
  }

  /**
   * Sets up Dictation's speech recognizer and various listeners.
   * @private
   */
  initialize_() {
    this.inputController_ = new InputController(() => this.stopDictation_());
    this.commandParser_ = new CommandParser();

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

    // Listen for Dictation toggles (activated / deactivated) from the Ash
    // Browser process.
    chrome.accessibilityPrivate.onToggleDictation.addListener(
        activated => this.onToggleDictation_(activated));

    chrome.accessibilityPrivate.isFeatureEnabled(
        chrome.accessibilityPrivate.AccessibilityFeature.DICTATION_COMMANDS,
        (result) => {
          this.commandsFeatureEnabled_ = result;
          if (this.commandsFeatureEnabled_) {
            this.commandParser_.setCommandsEnabled();
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
      this.speechRecognizer_.start();
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
    if (this.inputController_.hasCompositionText() || this.interimText_) {
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

    if (this.commandsFeatureEnabled_) {
      this.inputController_.commitText(this.interimText_);
      this.hideCommandsUI_();
    }
    this.inputController_.disconnect();
    Dictation.removeAsInputMethod();
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
    this.setStopTimeout_(
        result.isFinal ? Dictation.Timeouts.NO_SPEECH_MS :
                         Dictation.Timeouts.NO_NEW_SPEECH_MS);
  }

  /**
   * Processes a speech recognition result.
   * @param {string} transcript
   * @param {boolean} isFinal Whether this is a finalized transcript or an
   *     interim result.
   * @private
   */
  processSpeechRecognitionResult_(transcript, isFinal) {
    if (isFinal) {
      const command = this.commandParser_.parse(transcript);
      if (this.commandsFeatureEnabled_) {
        if (command.execute()) {
          this.showCommandExecuted_(command);
        } else {
          this.showCommandExecutionFailed_();
        }
      }
      if (command.isTextInput()) {
        this.inputController_.commitText(command.getText());
      }
    } else {
      if (this.commandsFeatureEnabled_) {
        this.setInterimText_(transcript);
      } else if (!this.chromeVoxEnabled_) {
        // When ChromeVox is enabled, we shouldn't populate interim
        // composition results because it will increase the verbosity too much.
        this.inputController_.setCompositionText(transcript);
      }
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
    // Display the "....".
    this.clearInterimText_();
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
   * TODO(crbug.com/1252037): Implement with final design instead of input.ime.
   * @param {string} text
   * @private
   */
  setInterimText_(text) {
    // TODO(crbug.com/1252037): Need to find a way to show interim text that is
    // only whitespace. Google Cloud Speech can return a newline character
    // although SODA does not seem to do that. The newline character looks wrong
    // here.
    this.interimText_ = text;
    if (this.chromeVoxEnabled_) {
      // Using chrome.input.ime for UI causes too much verbosity with ChromeVox.
      return;
    }
    this.inputController_.showAnnotation(this.interimText_);
    if (this.clearUITextTimeoutId_) {
      clearTimeout(this.clearUITextTimeoutId_);
      this.clearUITextTimeoutId_ = null;
    }
  }

  /**
   * Clears the interim result in the UI, replacing it with '....'.
   * TODO(crbug.com/1252037): Implement with final design instead of input.ime.
   * @private
   */
  clearInterimText_() {
    if (this.chromeVoxEnabled_) {
      // Using chrome.input.ime for UI causes too much verbosity with ChromeVox.
      return;
    }
    this.interimText_ = '';
    this.inputController_.showAnnotation('....');
    if (this.clearUITextTimeoutId_) {
      clearTimeout(this.clearUITextTimeoutId_);
      this.clearUITextTimeoutId_ = null;
    }
  }

  /**
   * Shows that a command was executed in the UI.
   * TODO(crbug.com/1252037): Implement with final design instead of input.ime.
   * @param {Command} command
   * @private
   */
  showCommandExecuted_(command) {
    if (this.chromeVoxEnabled_) {
      // Using chrome.input.ime for UI causes too much verbosity with ChromeVox.
      return;
    }
    if (command.isTextInput()) {
      // Return to the '....' UI.
      this.clearInterimText_();
      return;
    }
    this.interimText_ = '';
    this.inputController_.showAnnotation(
        '☑' + this.commandParser_.getCommandString(command));
    this.clearUITextTimeoutId_ = setTimeout(
        () => this.clearInterimText_(),
        Dictation.Timeouts.SHOW_COMMAND_MESSAGE_MS);
  }

  /**
   * Shows a message in the UI that a command failed to execute.
   * TODO(crbug.com/1252037): Implement with final design instead of input.ime.
   * @private
   */
  showCommandExecutionFailed_() {
    if (this.chromeVoxEnabled_) {
      // Using chrome.input.ime for UI causes too much verbosity with ChromeVox.
      return;
    }
    this.interimText_ = '';
    // TODO(crbug.com/1252037): Finalize string and internationalization.
    this.inputController_.showAnnotation(`ⓘ We didn't recognize that`);
    this.clearUITextTimeoutId_ = setTimeout(
        () => this.clearInterimText_(),
        Dictation.Timeouts.SHOW_COMMAND_MESSAGE_MS);
  }

  /**
   * Hides the commands UI bubble.
   * TODO(crbug.com/1252037): Implement with final design instead of input.ime.
   * @private
   */
  hideCommandsUI_() {
    if (this.chromeVoxEnabled_) {
      return;
    }
    this.inputController_.hideAnnotation();
    this.interimText_ = '';
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
 * @enum {!number}
 */
Dictation.DictationState = {
  OFF: 1,
  STARTING: 2,
  LISTENING: 3,
  STOPPING: 4,
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
